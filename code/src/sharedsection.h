//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/

#ifndef SHAREDSECTION_H
#define SHAREDSECTION_H

#include <QDebug>

#include <pcosynchro/pcosemaphore.h>

#ifdef USE_FAKE_LOCO
#  include "fake_locomotive.h"
#else
#  include "locomotive.h"
#endif

#ifndef USE_FAKE_LOCO
#include "ctrain_handler.h"
#endif

#include "sharedsectioninterface.h"

/**
 * @brief La classe SharedSection implémente l'interface SharedSectionInterface qui
 * propose les méthodes liées à la section partagée.
 */
class SharedSection final : public SharedSectionInterface
{
public:
    SharedSection()
        : _mutex(1),
        _semD1(0),
        _semD2(0),
        _noOccupant(0)
    {}

    /**
     * @brief Demande d'accès à la section partagée.
     *
     * Cas principaux :
     *  - Si arrêt d'urgence actif -> on refuse l'accès et on arrête la loco.
     *  - Si la loco est déjà inside -> erreur protocole.
     *  - Si la section est libre, pas de handoff et personne en attente ->
     *    entrée immédiate.
     *  - Sinon, la loco est mise dans la file d'attente de sa direction
     *    et bloquée sur le sémaphore correspondant.
     */
    void access(Locomotive& loco, Direction d) override
    {
        _mutex.acquire();

        // Arrêt d'urgence : on refuse tout nouvel accès.
        if (_emergency) {
            _mutex.release();
            loco.arreter();
            return;
        }

        // Même loco qui tente un deuxième access sans leave()
        if (_occupied && _owner == &loco) {
            ++_errors;
            _mutex.release();
            return;
        }

        bool hasWaiters = (_waitingD1 + _waitingD2) > 0;

        // Entrée immédiate uniquement si :
        //  - section libre
        //  - aucun handoff en cours
        //  - pas de file d’attente
        if (!_occupied && !_handoffInProgress && !hasWaiters) {
            _occupied  = true;
            _owner     = &loco;
            _ownerDir  = d;
            _everUsed  = true;
            _mutex.release();
            return;
        }

        // Sinon, on s’aligne dans la file de la bonne direction
        if (d == Direction::D1) {
            ++_waitingD1;
        } else {
            ++_waitingD2;
        }

        _mutex.release();

        loco.arreter();

        // Attente sur la file
        if (d == Direction::D1) {
            _semD1.acquire();
        } else {
            _semD2.acquire();
        }

        // Réveillé : on (re)prend la section
        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            return;
        }

        _occupied          = true;
        _owner             = &loco;
        _ownerDir          = d;
        _handoffInProgress = false; // ce thread consomme le handoff
        _everUsed          = true;

        _mutex.release();
    }

    /**
     * @brief Indique que la locomotive a physiquement quitté la section.
     *
     * Cas :
     *  - Si l'appel est incohérent (pas owner, mauvaise direction, section vide)
     *    -> erreur protocole.
     *  - En mode normal :
     *      * on libère la section
     *      * on réveille d’abord un attenteur de la direction opposée
     *        si disponible, sinon un attenteur de la même direction
     *        (et on active un handoff).
     *  - En mode urgence :
     *      * on libère la section
     *      * on notifie stopAll() via _noOccupant.
     */
    void leave(Locomotive& loco, Direction d) override
    {
        _mutex.acquire();

        if (!_occupied || _owner != &loco || _ownerDir != d) {
            ++_errors;
            _mutex.release();
            return;
        }

        _occupied = false;
        _owner    = nullptr;

        // Si on est en mode urgence : on signale juste la fin d’occupation
        if (_emergency) {
            _mutex.release();
            _noOccupant.release();    // réveille éventuellement stopAll()
            return;
        }

        int &oppWaiting  = (d == Direction::D1) ? _waitingD2 : _waitingD1;
        int &sameWaiting = (d == Direction::D1) ? _waitingD1 : _waitingD2;

        // Priorité à la direction opposée
        if (oppWaiting > 0) {
            _handoffInProgress = true;
            if (d == Direction::D1) {
                --_waitingD2;
                _semD2.release();
            } else {
                --_waitingD1;
                _semD1.release();
            }
        } else if (sameWaiting > 0) {
            _handoffInProgress = true;
            if (d == Direction::D1) {
                --_waitingD1;
                _semD1.release();
            } else {
                --_waitingD2;
                _semD2.release();
            }
        }
        // Sinon : personne à réveiller, section libre

        _mutex.release();
    }


    /**
     * @brief Notification supplémentaire après leave().
     *
     * Dans notre protocole, release() a un rôle très limité :
     *  - Si la section est libre ET il y a des attenteurs, on réveille un seul
     *    thread (et on met un handoff en place).
     *  - Sinon :
     *      * si la section n'a jamais été utilisée, c'est un mauvais usage,
     *        on incrémente le compteur d'erreurs (test ReleaseWithoutPending).
     *      * si la section a déjà servi, on ignore silencieusement.
     */
    void release(Locomotive& /*loco*/) override
    {
        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            return;
        }

        // Section libre + des gens en attente => on réveille une loco
        if (!_occupied && (_waitingD1 > 0 || _waitingD2 > 0)) {
            _handoffInProgress = true;
            if (_waitingD1 > 0) {
                --_waitingD1;
                _semD1.release();
            } else {
                --_waitingD2;
                _semD2.release();
            }
            _mutex.release();
            return;
        }

        // Personne à réveiller :
        //  - si jamais utilisée => mauvais usage (tests ReleaseWithoutPending)
        //  - sinon on ignore
        if (!_everUsed) {
            ++_errors;
        }

        _mutex.release();
    }

    /**
     * @brief Arrêt d'urgence :
     *  - Plus personne ne doit entrer.
     *  - Tous les attenteurs sont réveillés pour ne pas laisser de threads bloqués.
     *  - Si une loco est actuellement dans la section, on attend qu'elle finisse
     *    sa section critique et appelle leave() avant de retourner.
     */
    void stopAll() override
    {
        _mutex.acquire();
        _emergency = true;

        int n1 = _waitingD1;
        int n2 = _waitingD2;

        bool hadOccupant = _occupied;

        // On vide les files d’attente, mais on NE touche PAS à _occupied/_owner :
        // le train déjà dans la section finira sa section critique lui-même.
        _waitingD1 = 0;
        _waitingD2 = 0;

        _mutex.release();

        // Débloquer tous les threads en attente
        for (int i = 0; i < n1; ++i) _semD1.release();
        for (int i = 0; i < n2; ++i) _semD2.release();

        // S’il y avait un occupant au moment de l’arrêt d’urgence,
        // on attend qu’il sorte (leave() signalera _noOccupant).
        if (hadOccupant) {
            _noOccupant.acquire();
        }
    }

    /**
     * @brief Retourne le nombre d'erreurs détectées.
     *
     * Erreurs typiques :
     *  - access() consécutif par la même loco sans leave()
     *  - leave() appelé alors que la loco n’est pas propriétaire
     *  - release() appelé alors que la section n’a jamais été utilisée
     */
    int nbErrors() override
    {
        return _errors;
    }

private:
    // Sémaphores de synchronisation
    PcoSemaphore _mutex;        ///< Mutex général pour protéger l'état interne
    PcoSemaphore _semD1;        ///< File d'attente pour la direction D1
    PcoSemaphore _semD2;        ///< File d'attente pour la direction D2
    PcoSemaphore _noOccupant;   ///< Utilisé par stopAll() pour attendre la sortie de l’occupant

    // État de la section
    bool         _occupied {false};          ///< true si une loco est dans la section
    Locomotive*  _owner    {nullptr};        ///< pointeur vers la loco propriétaire actuelle
    Direction    _ownerDir {Direction::D1};  ///< direction de la loco propriétaire

    // Files d’attente (compteurs logiques)
    int  _waitingD1 {0};        ///< nombre de locos en attente en direction D1
    int  _waitingD2 {0};        ///< nombre de locos en attente en direction D2

    // Flags de contrôle
    bool _emergency         {false};  ///< mode arrêt d'urgence actif
    bool _handoffInProgress {false};  ///< un "handoff" a été réservé à une loco réveillée
    bool _everUsed          {false};  ///< true dès qu'au moins une loco est entrée une fois

    int  _errors            {0};      ///< compteur d'erreurs de protocol
};

#endif // SHAREDSECTION_H

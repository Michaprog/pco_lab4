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

    /**
     * @brief SharedSection Constructeur de la classe qui représente la section partagée.
     * Initialisez vos éventuels attributs ici, sémaphores etc.
     */
    SharedSection()
    : _mutex(1), _semD1(0), _semD2(0) {
        // TODO
    }

    /**
     * @brief Request access to the shared section
     * @param Locomotive who asked access
     * @param Direction of the locomotive
     */
    void access(Locomotive& loco, Direction d) override
    {
        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            loco.arreter();
            return;
        }

        const bool hasWaiters = (_waitingD1 + _waitingD2) > 0;
        if (!_occupied && !_handoffInProgress && !hasWaiters && !_pendingRelease) {
            _occupied = true;
            _owner    = &loco;
            _ownerDir = d;
            _mutex.release();
            return;
        }

        if (_owner == &loco) {
            ++_errors;
            _mutex.release();
            return;
        }

        if (d == Direction::D1) ++_waitingD1; else ++_waitingD2;
        _mutex.release();

        loco.arreter();

        if (d == Direction::D1) _semD1.acquire(); else _semD2.acquire();

        _mutex.acquire();
        if (_emergency) { _mutex.release(); return; }

        _occupied = true;
        _owner    = &loco;
        _ownerDir = d;
        _handoffInProgress = false;
        _mutex.release();
    }


    /**
     * @brief Notify the shared section that a Locomotive has left (not freed yed).
     * @param Locomotive who left
     * @param Direction of the locomotive
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
        _owner = nullptr;
        _lastLeaveDir = d;

        if (_emergency) {
            _pendingRelease = false;
            _awaitingReleaseFrom = nullptr;
            _mutex.release();
            return;
        }

        int& oppWaiting  = (d == Direction::D1) ? _waitingD2 : _waitingD1;
        int& sameWaiting = (d == Direction::D1) ? _waitingD1 : _waitingD2;

        if (oppWaiting > 0) {
            _handoffInProgress = true;
            if (d == Direction::D1) { --_waitingD2; _semD2.release(); }
            else                    { --_waitingD1; _semD1.release(); }
            _pendingRelease = false;
            _awaitingReleaseFrom = nullptr;
        } else if (sameWaiting > 0) {
            _pendingRelease = true;
            _awaitingReleaseFrom = &loco;
        } else {
            _pendingRelease = false;
            _awaitingReleaseFrom = nullptr;
        }

        _mutex.release();
    }



    /**
     * @brief Notify the shared section that it can now be accessed again (freed).
     * @param Locomotive who sent the notification
     */
    void release(Locomotive& loco) override
    {
        _mutex.acquire();

        Direction d = _lastLeaveDir;
        int& sameWaiting = (d == Direction::D1) ? _waitingD1 : _waitingD2;

        if (_pendingRelease) {
            if (_awaitingReleaseFrom && _awaitingReleaseFrom != &loco) {
                ++_errors;
            }

            if (sameWaiting > 0) {
                _handoffInProgress = true;
                if (d == Direction::D1) { --_waitingD1; _semD1.release(); }
                else                    { --_waitingD2; _semD2.release(); }
            } else {
                ++_errors;
            }
            _pendingRelease = false;
            _awaitingReleaseFrom = nullptr;
            _mutex.release();
            return;
        }

        if (sameWaiting > 0 && !_handoffInProgress) {
            _handoffInProgress = true;
            if (d == Direction::D1) { --_waitingD1; _semD1.release(); }
            else                    { --_waitingD2; _semD2.release(); }
            _mutex.release();
            return;
        }

        ++_errors;
        _mutex.release();
    }



    /**
     * @brief Stop all locomotives to access this shared section
     */
    void stopAll() override
    {
        _mutex.acquire();
        _emergency = true;

        int n1 = _waitingD1, n2 = _waitingD2;
        _waitingD1 = 0;
        _waitingD2 = 0;

        _pendingRelease = false;
        _awaitingReleaseFrom = nullptr;
        _handoffInProgress = false;

        _occupied = false;
        _owner = nullptr;

        _mutex.release();

        for (int i = 0; i < n1; ++i) _semD1.release();
        for (int i = 0; i < n2; ++i) _semD2.release();
    }


    /**
     * @brief Return nbErrors
     * @return nbErrors
     */
    int nbErrors() override {
        // TODO
        return _errors;
    }

private:
    /*
     * Vous êtes libres d'ajouter des méthodes ou attributs
     * pour implémenter la section partagée.
     */

    // Synchro
    PcoSemaphore _mutex;
    PcoSemaphore _semD1;
    PcoSemaphore _semD2;

    // Section d'etats
    bool _occupied {false};
    Locomotive* _owner {nullptr};
    Direction _ownerDir {Direction::D1};

    // Attendte de  directions
    int _waitingD1 {0};
    int _waitingD2 {0};

    // Regle d'acces et travail d'urgence
    bool _pendingRelease {false};
    Direction _lastLeaveDir {Direction::D1};
    bool _emergency {false};
    bool _handoffInProgress {false};
    Locomotive* _awaitingReleaseFrom {nullptr};


    // Compteur d'erreures
    int _errors {0};

};


#endif // SHAREDSECTION_H

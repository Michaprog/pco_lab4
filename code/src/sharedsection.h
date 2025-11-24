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
    : _mutex(1), _semD1(0), _semD2(0), 
      _waitingD1(0), _waitingD2(0), 
      _inD1(false), _inD2(false),
      _errorCount(0) {
    }

    /**
     * @brief Méthode appelée lorsqu'une locomotive souhaite accéder à la section partagée
     * @param loco La locomotive qui demande l'accès
     * @param d La direction de la locomotive
     */
    void access(Locomotive& loco, Direction d) override {
        _mutex.acquire();
        
        if ((d == Direction::D1 && _inD1) || (d == Direction::D2 && _inD2)) {
            // Already in the section in the same direction, no need to wait
            _mutex.release();
            return;
        }

        if (d == Direction::D1) {
            _waitingD1++;
            _mutex.release();
            _semD1.acquire();
            _mutex.acquire();
            _waitingD1--;
            _inD1 = true;
        } else {
            _waitingD2++;
            _mutex.release();
            _semD2.acquire();
            _mutex.acquire();
            _waitingD2--;
            _inD2 = true;
        }
        
        _mutex.release();
    }

    /**
     * @brief Méthode appelée lorsqu'une locomotive quitte la section partagée
     * @param loco La locomotive qui quitte la section
     * @param d La direction de la locomotive
     */
    void leave(Locomotive& loco, Direction d) override {
        _mutex.acquire();
        
        if (d == Direction::D1) {
            if (!_inD1) {
                _errorCount++;
                _mutex.release();
                return;
            }
            _inD1 = false;
            
            // If there are trains waiting in the opposite direction, let them pass first
            if (_waitingD2 > 0) {
                _semD2.release();
            } else if (_waitingD1 > 0) {
                // If no opposite direction, let same direction pass
                _semD1.release();
            } else {
                _mutex.release();
            }
        } else {
            if (!_inD2) {
                _errorCount++;
                _mutex.release();
                return;
            }
            _inD2 = false;
            
            // If there are trains waiting in the opposite direction, let them pass first
            if (_waitingD1 > 0) {
                _semD1.release();
            } else if (_waitingD2 > 0) {
                // If no opposite direction, let same direction pass
                _semD2.release();
            } else {
                _mutex.release();
            }
        }
    }

    /**
     * @brief Méthode appelée pour libérer la section partagée après un leave()
     * @param loco La locomotive qui libère la section
     */
    void release(Locomotive& loco) override {
        _mutex.acquire();
        
        // If the train was going in D1 direction, check if there are D2 trains waiting
        if (_inD1 && _waitingD2 > 0) {
            _semD2.release();
        } 
        // If the train was going in D2 direction, check if there are D1 trains waiting
        else if (_inD2 && _waitingD1 > 0) {
            _semD1.release();
        }
        // If no opposite direction trains, release the mutex
        else {
            _mutex.release();
        }
    }

    /**
     * @brief Arrête toutes les locomotives qui attendent d'accéder à la section partagée
     */
    void stopAll() override {
        _mutex.acquire();
        
        // Release all waiting locomotives
        while (_waitingD1 > 0) {
            _semD1.release();
            _waitingD1--;
        }
        while (_waitingD2 > 0) {
            _semD2.release();
            _waitingD2--;
        }
        
        _inD1 = false;
        _inD2 = false;
        _mutex.release();
    }

    /**
     * @brief Retourne le nombre d'erreurs de synchronisation détectées
     * @return Le nombre d'erreurs
     */
    int nbErrors() override {
        return _errorCount;
    }

private:
    PcoSemaphore _mutex;      // Mutex pour les sections critiques
    PcoSemaphore _semD1;      // Sémaphore pour la direction D1
    PcoSemaphore _semD2;      // Sémaphore pour la direction D2
    int _waitingD1;           // Nombre de locomotives en attente en direction D1
    int _waitingD2;           // Nombre de locomotives en attente en direction D2
    bool _inD1;               // Une locomotive est-elle dans la section en direction D1 ?
    bool _inD2;               // Une locomotive est-elle dans la section en direction D2 ?
    int _errorCount;          // Compteur d'erreurs de synchronisation
};

#endif // SHAREDSECTION_H

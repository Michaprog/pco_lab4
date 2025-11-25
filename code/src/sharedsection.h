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
            : _mutex(1), _semD1(0), _semD2(0)
    {}

    void access(Locomotive& loco, Direction d) override
    {
        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            loco.arreter();
            return;
        }

        if (_occupied && _owner == &loco) {
            ++_errors;
            _mutex.release();
            return;
        }

        if (!_occupied && _waitingD1 == 0 && _waitingD2 == 0) {
            _occupied = true;
            _owner    = &loco;
            _ownerDir = d;
            _mutex.release();
            return;
        }

        if (d == Direction::D1) {
            ++_waitingD1;
        } else {
            ++_waitingD2;
        }

        _mutex.release();

        loco.arreter();

        if (d == Direction::D1) {
            _semD1.acquire();
        } else {
            _semD2.acquire();
        }

        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            return;
        }

        _occupied = true;
        _owner    = &loco;
        _ownerDir = d;

        _mutex.release();
    }

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

        if (_emergency) {
            _mutex.release();
            return;
        }

        int &oppWaiting  = (d == Direction::D1) ? _waitingD2 : _waitingD1;
        int &sameWaiting = (d == Direction::D1) ? _waitingD1 : _waitingD2;

        if (oppWaiting > 0) {
            if (d == Direction::D1) {
                --_waitingD2;
                _semD2.release();
            } else {
                --_waitingD1;
                _semD1.release();
            }
        } else if (sameWaiting > 0) {
            if (d == Direction::D1) {
                --_waitingD1;
                _semD1.release();
            } else {
                --_waitingD2;
                _semD2.release();
            }
        }

        _mutex.release();
    }

    void release(Locomotive& loco) override
    {
        _mutex.acquire();

        if (_emergency) {
            _mutex.release();
            return;
        }

        if (!_occupied) {
            if (_waitingD1 > 0) {
                --_waitingD1;
                _semD1.release();
            } else if (_waitingD2 > 0) {
                --_waitingD2;
                _semD2.release();
            }
        }

        _mutex.release();
    }

    void stopAll() override
    {
        _mutex.acquire();
        _emergency = true;

        int n1 = _waitingD1;
        int n2 = _waitingD2;
        _waitingD1 = 0;
        _waitingD2 = 0;

        _occupied = false;
        _owner    = nullptr;

        _mutex.release();

        for (int i = 0; i < n1; ++i) _semD1.release();
        for (int i = 0; i < n2; ++i) _semD2.release();
    }

    int nbErrors() override
    {
        return _errors;
    }

private:
    PcoSemaphore _mutex;
    PcoSemaphore _semD1;
    PcoSemaphore _semD2;

    bool _occupied {false};
    Locomotive* _owner {nullptr};
    Direction _ownerDir {Direction::D1};

    int _waitingD1 {0};
    int _waitingD2 {0};

    bool _emergency {false};
    int  _errors {0};
};

#endif // SHAREDSECTION_H
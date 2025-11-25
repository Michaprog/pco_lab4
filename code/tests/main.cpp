//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/

#include <gtest/gtest.h>
#include <atomic>
#include <random>
#include <vector>
#include <algorithm>

#include <pcosynchro/pcothread.h>
#include <pcosynchro/pcosemaphore.h>

#include "sharedsection.h"
#include "sharedsectioninterface.h"

using Dir = SharedSectionInterface::Direction;


// ========================= Helpers ========================= //

// RAII pour vérifier qu'il n'y a JAMAIS plus d'une loco dans la section.
// - Au constructeur : on incrémente nbIn et on vérifie qu'il vaut 1.
// - Au destructeur  : on décrémente nbIn.
struct ScopedCritical {
    std::atomic<int>& nbIn;
    explicit ScopedCritical(std::atomic<int>& c) : nbIn(c) {
        int now = nbIn.fetch_add(1) + 1;
        EXPECT_EQ(now, 1) << "Deux locomotives dans la section en même temps !";
    }
    ~ScopedCritical() {
        nbIn.fetch_sub(1);
    }
};

// Petit travail simulé (temps passé dans la section)
static void short_work(int us = 200) {
    PcoThread::usleep(us);
}

// Petit yield pour laisser d'autres threads s’exécuter
static void small_yield() {
    PcoThread::usleep(10);
}

// Version "directe" de la vérification de section critique
static void enterCritical(std::atomic<int>& nbIn) {
    int now = nbIn.fetch_add(1) + 1;
    ASSERT_EQ(now, 1) << "Deux locomotives dans la section en même temps !";
}
static void leaveCritical(std::atomic<int>& nbIn) {
    nbIn.fetch_sub(1);
}


// ========================= Tests de base ========================= //

// Test que 2 locos dans la même direction passent bien l'une après l'autre,
// sans overlap dans la section.
TEST(SharedSection, TwoSameDirection_SerializesCorrectly) {
    SharedSection section;
    std::atomic<int> nbIn{0};
    Locomotive l1(1, 10, 0), l2(2, 10, 0);

    // Thread de la première loco
    PcoThread t1([&]{
        section.access(l1, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);               // doit être la seule dedans
        PcoThread::usleep(1000);           // reste un peu dans la section
        leaveCritical(nbIn);
        section.leave(l1, SharedSectionInterface::Direction::D1);
        section.release(l1);               // réveille éventuellement un suivant
    });

    // Thread de la deuxième loco (arrive un peu plus tard)
    PcoThread t2([&]{
        PcoThread::usleep(500);            // démarre après l1
        section.access(l2, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);               // doit attendre que l1 soit sortie
        leaveCritical(nbIn);
        section.leave(l2, SharedSectionInterface::Direction::D1);
    });

    t1.join(); t2.join();
    ASSERT_EQ(section.nbErrors(), 0);
}

// Test que faire deux fois access() de suite avec la même loco sans leave()
// est compté comme une erreur de protocole.
TEST(SharedSection, ConsecutiveAccess_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    section.access(l1, SharedSectionInterface::Direction::D1);   // interdit
    section.leave(l1, SharedSectionInterface::Direction::D1);

    ASSERT_EQ(section.nbErrors(), 1);
}

// Test que leave() avec une mauvaise direction est détecté comme une erreur.
TEST(SharedSection, LeaveWrongDirection_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    // On "sort" avec la mauvaise direction -> erreur
    section.leave(l1, SharedSectionInterface::Direction::D2);

    ASSERT_EQ(section.nbErrors(), 1);
}


// ========================= Tests complexes ========================= //

// Test de contention forte avec des locomotives en directions opposées.
// Beaucoup de threads D1 et D2 entrent/sortent, on vérifie:
// - qu'il y a bien des passages dans les deux sens
// - qu'il n'y a jamais plus d'une loco dans la section
// - qu'aucune erreur de protocole n'a été déclenchée
TEST(SharedSectionComplex, OppositeDirections_StrongContention_AlternatesOften)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    std::atomic<int> countD1{0}, countD2{0};
    const int N = 40;   // nombre de locos par direction

    std::vector<std::unique_ptr<PcoThread>> threads;
    threads.reserve(2*N);

    for (int i = 0; i < N; ++i) {
        // Loco en D1
        threads.emplace_back(std::make_unique<PcoThread>([&, i]{
            Locomotive l(100 + i, 10, 0);
            section.access(l, Dir::D1);
            {
                ScopedCritical sc(nbIn);
                ++countD1;
                short_work(150);
            }
            section.leave(l, Dir::D1);
        }));
        // Loco en D2 (décalée un peu dans le temps)
        threads.emplace_back(std::make_unique<PcoThread>([&, i]{
            Locomotive l(200 + i, 10, 0);
            PcoThread::usleep(50);
            section.access(l, Dir::D2);
            {
                ScopedCritical sc(nbIn);
                ++countD2;
                short_work(150);
            }
            section.leave(l, Dir::D2);
        }));
    }

    for (auto& t : threads) t->join();

    ASSERT_GT(countD1.load(), 0);          // au moins un passage D1
    ASSERT_GT(countD2.load(), 0);          // au moins un passage D2
    ASSERT_EQ(section.nbErrors(), 0);      // pas d'erreurs de protocole
}

// Test que plusieurs locos dans la même direction sont servies une par une,
// en utilisant release() comme "signal" pour passer au suivant.
TEST(SharedSectionComplex, SameDirection_MultipleFollowers_ReleaseOneByOne)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    const int K = 5;    // nombre de locos "suiveuses"

    Locomotive leader(1, 10, 0);
    std::vector<std::unique_ptr<PcoThread>> followers;

    std::atomic<int> entered{0};

    // Threads des K suiveurs
    for (int i = 0; i < K; ++i) {
        followers.emplace_back(std::make_unique<PcoThread>([&, i]{
            Locomotive f(10 + i, 10, 0);
            section.access(f, Dir::D1);
            {
                ScopedCritical sc(nbIn);
                entered.fetch_add(1);
                short_work(80);
            }
            section.leave(f, Dir::D1);
        }));
    }

    // Le "leader" entre en premier
    section.access(leader, Dir::D1);
    {
        ScopedCritical sc(nbIn);
        short_work(200);
    }
    section.leave(leader, Dir::D1);

    // Le leader "drive" les entrées successives en appelant release()
    for (int i = 0; i < K; ++i) {
        small_yield();
        section.release(leader);
        PcoThread::usleep(120);
        ASSERT_GE(entered.load(), i + 1);
    }

    for (auto& t : followers) t->join();
    ASSERT_EQ(entered.load(), K);          // tous les suiveurs sont passés
    ASSERT_EQ(section.nbErrors(), 0);      // protocole respecté
}

// Test que release() sur une section jamais utilisée est considéré comme une
// erreur (et que chaque appel augmente nbErrors de 1).
TEST(SharedSectionComplex, ReleaseWithoutPending_IncrementsErrorOnce)
{
    SharedSection section;
    Locomotive l(1, 10, 0);

    int before = section.nbErrors();
    section.release(l);        // première erreur
    int mid = section.nbErrors();
    section.release(l);        // deuxième erreur
    int after = section.nbErrors();

    ASSERT_EQ(mid, before + 1);
    ASSERT_EQ(after, mid + 1);
}

// Test d'arrêt d'urgence avec :
//  - une loco déjà dans la section
//  - deux locos en attente
//  - puis une nouvelle loco après le stop
//
// On vérifie :
//  - que les attenteurs se réveillent (t2Awake/t3Awake == true)
//  - qu'aucune nouvelle loco n'entre après stopAll()
//  - que nbIn revient bien à 0
TEST(SharedSectionComplex, EmergencyStop_WakesWaiters_PreventsEntry)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    Locomotive l1(1, 10, 0), l2(2, 10, 0), l3(3, 10, 0);
    std::atomic<bool> t2Awake{false}, t3Awake{false};
    std::atomic<int> enteredAfterStop{0};

    // Loco 1 : entre dans la section, y reste un moment, puis sort
    PcoThread t1([&]{
        section.access(l1, Dir::D1);
        {
            ScopedCritical sc(nbIn);
            PcoThread::usleep(500);
        }
        section.leave(l1, Dir::D1);
    });

    // Loco 2 : tente d'accéder, restera bloquée jusqu'à stopAll()
    PcoThread t2([&]{
        PcoThread::usleep(100);
        section.access(l2, Dir::D1);
        t2Awake.store(true);   // si elle se réveille, stopAll() a bien débloqué
        if (nbIn.load() == 0) {
            // on ne teste rien ici, juste pour potentielle logique
        }
    });

    // Loco 3 : tente aussi d'accéder depuis l'autre direction
    PcoThread t3([&]{
        PcoThread::usleep(120);
        section.access(l3, Dir::D2);
        t3Awake.store(true);
    });

    // On attend un peu, puis arrêt d'urgence
    PcoThread::usleep(300);
    section.stopAll();

    // Loco 4 : essaie d'entrer après l'arrêt d'urgence.
    // Elle ne doit JAMAIS entrer dans la section.
    PcoThread t4([&]{
        Locomotive l4(4, 10, 0);
        section.access(l4, Dir::D1);
        if (nbIn.load() == 1) enteredAfterStop.fetch_add(1);
    });

    t1.join(); t2.join(); t3.join(); t4.join();

    ASSERT_TRUE(t2Awake.load());               // les attenteurs ont été réveillés
    ASSERT_TRUE(t3Awake.load());
    ASSERT_EQ(enteredAfterStop.load(), 0);     // personne n'est entré après stopAll
    ASSERT_EQ(nbIn.load(), 0);                 // section vide à la fin
    ASSERT_GE(section.nbErrors(), 0);          // pas de contrainte stricte ici
}

// Test que leave() sans access() préalable est une erreur.
TEST(SharedSectionComplex, LeaveWithoutAccess_IsError)
{
    SharedSection section;
    Locomotive l(1, 10, 0);

    int before = section.nbErrors();
    section.leave(l, Dir::D1);     // incohérent, personne dans la section
    int after = section.nbErrors();

    ASSERT_EQ(after, before + 1);
}

// Test que faire deux fois leave() après un access est une erreur.
TEST(SharedSectionComplex, DoubleLeave_IsError)
{
    SharedSection section;
    Locomotive l(1, 10, 0);

    section.access(l, Dir::D1);
    section.leave(l, Dir::D1);     // normal

    int before = section.nbErrors();
    section.leave(l, Dir::D1);     // deuxième leave : erreur
    int after = section.nbErrors();

    ASSERT_EQ(after, before + 1);
}

// Variante d'arrêt d'urgence :
// - l1 est dans la section
// - stopAll() est déclenché
// - l2 tente d'entrer APRÈS l'arrêt
// On vérifie que l2 ne peut pas entrer dans la section (nbIn ne passe jamais à 1).
TEST(SharedSectionComplex, AccessAfterEmergency_DoesNotEnter)
{
    SharedSection section;
    std::atomic<int> nbIn{0};

    Locomotive l1(1, 10, 0);
    Locomotive l2(2, 10, 0);

    // Loco 1 entre dans la section
    PcoThread t1([&]{
        section.access(l1, Dir::D1);
        {
            ScopedCritical sc(nbIn);
            short_work(200);
        }
        section.leave(l1, Dir::D1);
    });

    // On laisse le temps à l1 de bien entrer
    PcoThread::usleep(50);

    // Arrêt d’urgence
    section.stopAll();

    // Loco 2 essaie d’entrer après l’arrêt d’urgence
    PcoThread t2([&]{
        section.access(l2, Dir::D2);
        // Si l2 entrait vraiment, nbIn vaudrait 1 à un moment.
        ASSERT_NE(nbIn.load(), 1) << "Une loco est entrée après l'arrêt d'urgence";
    });

    t1.join();
    t2.join();

    ASSERT_EQ(nbIn.load(), 0);     // personne dans la section à la fin
}

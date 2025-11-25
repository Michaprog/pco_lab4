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


// Les helpers
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

static void short_work(int us = 200) {
    PcoThread::usleep(us);
}

static void small_yield() {
    PcoThread::usleep(10);
}

static void enterCritical(std::atomic<int>& nbIn) {
    int now = nbIn.fetch_add(1) + 1;
    ASSERT_EQ(now, 1) << "Deux locomotives dans la section en même temps !";
}
static void leaveCritical(std::atomic<int>& nbIn) {
    nbIn.fetch_sub(1);
}

TEST(SharedSection, TwoSameDirection_SerializesCorrectly) {
    SharedSection section;
    std::atomic<int> nbIn{0};
    Locomotive l1(1, 10, 0), l2(2, 10, 0);

    PcoThread t1([&]{
        section.access(l1, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);
        PcoThread::usleep(1000);
        leaveCritical(nbIn);
        section.leave(l1, SharedSectionInterface::Direction::D1);
        section.release(l1);
    });

    PcoThread t2([&]{
        PcoThread::usleep(500);
        section.access(l2, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);
        leaveCritical(nbIn);
        section.leave(l2, SharedSectionInterface::Direction::D1);
    });

    t1.join(); t2.join();
    ASSERT_EQ(section.nbErrors(), 0);
}

TEST(SharedSection, ConsecutiveAccess_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    section.access(l1, SharedSectionInterface::Direction::D1);
    section.leave(l1, SharedSectionInterface::Direction::D1);

    ASSERT_EQ(section.nbErrors(), 1);
}

TEST(SharedSection, LeaveWrongDirection_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    section.leave(l1, SharedSectionInterface::Direction::D2); 

    ASSERT_EQ(section.nbErrors(), 1);
}


// ------- Test perso
// Direction opposées
TEST(SharedSectionComplex, OppositeDirections_StrongContention_AlternatesOften)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    std::atomic<int> countD1{0}, countD2{0};
    const int N = 40;

    std::vector<std::unique_ptr<PcoThread>> threads;
    threads.reserve(2*N);

    for (int i = 0; i < N; ++i) {
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

    ASSERT_GT(countD1.load(), 0);
    ASSERT_GT(countD2.load(), 0);
    ASSERT_EQ(section.nbErrors(), 0);
}

// meme direcrion  admis un par  un
TEST(SharedSectionComplex, SameDirection_MultipleFollowers_ReleaseOneByOne)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    const int K = 5;

    Locomotive leader(1, 10, 0);
    std::vector<std::unique_ptr<PcoThread>> followers;

    std::atomic<int> entered{0};
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

    // leader  part en premier
    section.access(leader, Dir::D1);
    {
        ScopedCritical sc(nbIn);
        short_work(200);
    }
    section.leave(leader, Dir::D1);

    for (int i = 0; i < K; ++i) {
        small_yield();
        section.release(leader);
        PcoThread::usleep(120);
        ASSERT_GE(entered.load(), i + 1);
    }

    for (auto& t : followers) t->join();
    ASSERT_EQ(entered.load(), K);
    ASSERT_EQ(section.nbErrors(), 0);
}

TEST(SharedSectionComplex, ReleaseWithoutPending_IncrementsErrorOnce)
{
    SharedSection section;
    Locomotive l(1, 10, 0);

    int before = section.nbErrors();
    section.release(l);
    int mid = section.nbErrors();
    section.release(l);
    int after = section.nbErrors();

    ASSERT_EQ(mid, before + 1);
    ASSERT_EQ(after, mid + 1);
}

TEST(SharedSectionComplex, EmergencyStop_WakesWaiters_PreventsEntry)
{
    SharedSection section;
    std::atomic<int> nbIn{0};
    Locomotive l1(1, 10, 0), l2(2, 10, 0), l3(3, 10, 0);
    std::atomic<bool> t2Awake{false}, t3Awake{false};
    std::atomic<int> enteredAfterStop{0};

    PcoThread t1([&]{
        section.access(l1, Dir::D1);
        {
            ScopedCritical sc(nbIn);
            PcoThread::usleep(500);
        }
        section.leave(l1, Dir::D1);
    });

    PcoThread t2([&]{
        PcoThread::usleep(100);
        section.access(l2, Dir::D1);
        t2Awake.store(true);
        if (nbIn.load() == 0) {
        }
    });
    PcoThread t3([&]{
        PcoThread::usleep(120);
        section.access(l3, Dir::D2);
        t3Awake.store(true);
    });

    PcoThread::usleep(300);
    section.stopAll();

    PcoThread t4([&]{
        Locomotive l4(4, 10, 0);
        section.access(l4, Dir::D1);
        if (nbIn.load() == 1) enteredAfterStop.fetch_add(1);
    });

    t1.join(); t2.join(); t3.join(); t4.join();

    ASSERT_TRUE(t2Awake.load());
    ASSERT_TRUE(t3Awake.load());
    ASSERT_EQ(enteredAfterStop.load(), 0);
    ASSERT_EQ(nbIn.load(), 0);
    ASSERT_GE(section.nbErrors(), 0);
}






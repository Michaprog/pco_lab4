//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 

#pragma once
#include <atomic>

class Locomotive {
public:
    Locomotive() = default;
    Locomotive(int id, int /*speed*/, int /*rev*/ = 0) : m_id(id) {}

    virtual ~Locomotive() = default;

    virtual void arreter()  { ++stops_;   running_ = false; }
    virtual void demarrer() { ++starts_;  running_ = true;  }

    int id() const { return m_id; }
    bool running() const { return running_; }
    int stops()  const { return stops_.load(); }
    int starts() const { return starts_.load(); }

private:
    int m_id{0};
    std::atomic<int> stops_{0};
    std::atomic<int> starts_{0};
    std::atomic<bool> running_{false};
};

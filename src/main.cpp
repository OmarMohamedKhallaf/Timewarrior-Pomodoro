#include <iostream>
#include <future>
#include <iomanip>

#include "utils.h"
#include "config.h"

int main() {
    utils::Ncurses ncurses;     // handle initialization of ncurses
    utils::Ncurses::Screen cmdScreen(stdscr);
    utils::Ncurses::Screen tmrScreen(LINES - 2, COLS - 2, 1, 1);
    utils::Queue<PomodoroSession<long, std::nano>> taskQueue;
    std::atomic<bool> isRunning = true;
    std::atomic<bool> isPause = false;

    std::thread worker([&] {
        std::chrono::duration<int, std::nano> delta(0);
        while (isRunning) {
            auto task{taskQueue.pop()};
            if (task.sessionType == PomodoroSessionType::WORK) {
                auto pid{vfork()};
                switch (pid) {
                    case -1:
                        // TODO: handle errors
                        break;
                    case 0:
                        fclose(stdin);
                        fclose(stdout);
                        fclose(stderr);
                        execl("/usr/bin/timew", "continue", nullptr);
                        break;
                    default:
                        break;
                }
            }
            // timer
            auto prevTime{std::chrono::steady_clock::now()};
            while (task.duration.count() > 0 && isRunning && !isPause) {
                // TODO: use ascii art to print digits adapted to the size of the terminal
                tmrScreen.putStringAtLine(utils::formatSeconds<long, std::nano>(task.duration), 1,
                                          tmrScreen.getCols() / 2 - 5);
                tmrScreen.render();
                auto sleepTime{std::chrono::seconds(1) - delta};            // desired sleep time
                std::this_thread::sleep_for(std::chrono::duration<long, std::nano>(sleepTime));
                auto curTime = std::chrono::steady_clock::now();
                auto timeSlept = curTime - prevTime;                        // actual slept time
                delta = (timeSlept - sleepTime) % std::chrono::seconds(1);  // fraction of second
                task.duration -= timeSlept;
                prevTime = curTime;
            }
            if (task.sessionType == PomodoroSessionType::WORK) {
                auto pid{vfork()};
                switch (pid) {
                    case -1:
                        // TODO: handle errors
                        break;
                    case 0:
                        fclose(stdin);
                        fclose(stdout);
                        fclose(stderr);
                        execl("/usr/bin/timew", "stop", nullptr);
                        break;
                    default:
                        break;
                }
            }
            tmrScreen.clearScreen();
        }
    });

    unsigned int counter = 120;

    auto cmdChar{'\0'};
    cmdScreen.putStringAtLine("commands: (s)tart, (p)ause, (e)xit", 0, COLS / 2 - 17);
    cmdScreen.render();
    while ((cmdChar = cmdScreen.getCharLower()) != 'e') {
        switch (cmdChar) {
            case 's':
                if (taskQueue.empty()) {
                    isPause = false;
                    taskQueue.push(
                            {std::chrono::duration<long, std::ratio<60, 1>>(25),PomodoroSessionType::WORK});
                    taskQueue.push(
                            {std::chrono::duration<long, std::ratio<60, 1>>(25), PomodoroSessionType::FREE});
                } else {
                    cmdScreen.putStringAtFor("A timer is already running", LINES - 1, 0,
                                             std::chrono::duration<int, std::ratio<1, 1>>(1));
                }
                break;
            case 'p':
                isPause = true;
                break;
            default:
                break;
        }
        flushinp();
    }

    isRunning = false;
    taskQueue.push({});    // necessary since the thread waits on the queue
    worker.join();

    return 0;
}
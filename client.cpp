#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <Ice/Initialize.h>
#include <thread>
#include <string>
#include <Ice/Proxy.h>
#include <memory>
#include <algorithm>

using namespace std;
using namespace MPK;

class UserI : public User {

public:

    void updateStop(TramPrxPtr tram, ::std::shared_ptr <StopPrx> stop, const ::Ice::Current &current) {
        for (auto myStop:tram->getSchedule()) {
            cout << tram->getID() << " Subscription from " << myStop.stop->getName() << ", next tram arrives at "
                 << myStop.time.hour << ":"
                 << myStop.time.minute << endl;
        }
    }

    void updateSchedule(StopPrxPtr stop, arrivals arr, const ::Ice::Current &current) {
        for (auto myTram:stop->getArrivals()) {
            cout << "Tram: " << myTram.tram->getID() << " will soon reach " << stop->getName() << " at "
                 << myTram.time.hour << ":" << myTram.time.minute << endl;
        }
    }


};

int main(int argc, char *argv[]) {
    Ice::CtrlCHandler ctrlCHandler;
    try {
        if (argc < 5) {
            cout << "Incorrect invocation parameters SERVER_ADDR SERVER_PORT SERVER_NAME PORT" << endl;
            return 1;
        }

        Ice::CommunicatorHolder ich(argc, argv);
        auto adapter = ich->createObjectAdapterWithEndpoints("", string("default -p ") + argv[4]);
        auto sip_proxy = Ice::checkedCast<SIPPrx>(
                ich->stringToProxy(string(argv[3]) + ":default -h " + argv[1] + " -p " + argv[2]));
        auto user_proxy = Ice::checkedCast<UserPrx>(adapter->addWithUUID(make_shared<UserI>()));
        adapter->activate();

        vector <TramPrxPtr> subscribed_trams;
        vector <StopPrxPtr> subscribed_stops;

        ctrlCHandler.setCallback([&user_proxy, &ich, &subscribed_stops, &subscribed_trams](int signal) {
            for (auto tram: subscribed_trams) {
                tram->unregisterUser(user_proxy);
            }
            for (auto stop: subscribed_stops) {
                stop->unregisterUser(user_proxy);
            }
            ich.communicator()->shutdown();
            quick_exit(0);
        });

        while (true) {
            char option;
            while (true) {
                cout << "Options:" << endl << "[S] - Subscribe to a stop or train" << endl
                     << "[C] - I want to cancel subscription" << endl;
                cin >> option;
                if (cin.fail() || (option != 'S' && option != 's' && option != 'C' && option != 'c')) {
                    cout << "There's no such an option" << endl;
                    if (cin.fail()) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    }
                    continue;
                }
                break;
            }
            switch (option) {
                case 'S':
                case 's': {
                    auto lines = sip_proxy->getLines();
                    cout << "Please, select a tram or a stop to subscribe, press [q 0] to leave:" << endl;
                    for (auto line:lines) {
                        for (auto stop : line->getStops()) {
                            bool found = false;

                            for (const auto &subscribed_stop: subscribed_stops) {
                                if (*subscribed_stop == *stop) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                cout << "stop " << stop->getID() << " " << stop->getName() << endl;
                            }
                        }
                    }
                    for (auto line:lines) {
                        for (auto tram : line->getTrams()) {
                            bool found = false;

                            for (const auto &subscribed_tram: subscribed_trams) {
                                if (*subscribed_tram == *tram) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                cout << "tram " << tram->getID() << endl;
                            }
                        }
                    }
                    string subChoice;
                    int id;

                    while (true) {
                        cin >> subChoice >> id;
                        if (cin.fail() || (subChoice != "tram" && subChoice != "stop" && subChoice != "q")) {
                            cout << "No such tram nor a stop" << endl;
                            if (cin.fail()) {
                                cin.clear();
                                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            }
                            cout << "Please enter the object to subscribe once again" << endl;
                            continue;
                        }
                        break;
                    }
                    if (subChoice == "q")break;

                    if (subChoice == "stop") {
                        if (!sip_proxy->getStop(id)) {
                            cout << "No such stop" << endl;
                        }
                        sip_proxy->getStop(id)->registerUser(user_proxy);
                        subscribed_stops.push_back(sip_proxy->getStop(id));
                    } else if (subChoice == "tram") {
                        for (auto line:lines) {
                            for (auto tram : line->getTrams()) {
                                if (tram->getID() == id) {
                                    tram->registerUser(user_proxy);
                                    subscribed_trams.push_back(tram);
                                }
                            }
                        }
                    }
                    break;
                }
                case 'c':
                case 'C': {
                    cout << "Please, select a subscription to cancel, press [q 0] to leave::" << endl;
                    for (auto stop:subscribed_stops) {
                        cout << "stop " << stop->getID() << " " << stop->getName() << endl;
                    }
                    for (auto it = subscribed_trams.begin(); it != subscribed_trams.end(); ) {
                        try {
                            int id = (*it)->getID();
                            std::cout << "tram " << id << std::endl;
                            ++it;
                        } catch (const std::exception& e) {
                            std::cerr << "The tram has finished Its route. Removing tram from subscribed_trams." << std::endl;
                            it = subscribed_trams.erase(it);
                        }
                    }
                    string subChoice;
                    int id;

                    while (true) {
                        cin >> subChoice >> id;
                        if (cin.fail() || (subChoice != "tram" && subChoice != "stop" && subChoice != "q")) {
                            cout << "No such tram nor a stop" << endl;
                            if (cin.fail()) {
                                cin.clear();
                                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            }
                            cout << "Please enter the object to subscribe once again" << endl;
                            continue;
                        }
                        break;
                    }
                    if (subChoice == "q")break;

                    if (subChoice == "stop") {
                        sip_proxy->getStop(id)->unregisterUser(user_proxy);
                        auto it = find(subscribed_stops.begin(), subscribed_stops.end(), sip_proxy->getStop(id));
                        subscribed_stops.erase(it);

                    } else if (subChoice == "tram") {
                        for (auto tram:subscribed_trams) {
                            if (tram->getID() == id) {
                                tram->unregisterUser(user_proxy);
                                auto it = find(subscribed_trams.begin(), subscribed_trams.end(), tram);
                                subscribed_trams.erase(it);
                            }

                        }
                    }
                    break;
                }

            }
        }


    }
    catch (const std::exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <Ice/Initialize.h>
#include <thread>
#include <Ice/Proxy.h>
#include <cstdlib>
#include <random>

using namespace std;
using namespace MPK;

TramPrxPtr tram_proxy;

class TramI: public Tram{

vector<UserPrxPtr> registered_users;
int tram_id;
LinePrxPtr currentLine;
schedule tram_shedule;
TimeOfDay startTime;
public:

    StopPrxPtr getStop(const ::Ice::Current& current){
        if(!tram_shedule.empty()){
            return tram_shedule[0].stop;
        }else{
            return NULL;
        }
    }

    void startTram(int id, LinePrxPtr line, TimeOfDay startTime){
        this->tram_id = id;
        this->currentLine = line;
        this->startTime=startTime;
        auto tram_stops = currentLine->getStops();
        line->addTram(tram_proxy);        
    }

    TimeOfDay getStopTime(int stopid, const ::Ice::Current& current){
        auto pos = find_if(tram_shedule.cbegin(), tram_shedule.cend(), [&stopid](ScheduleItem s){return s.stop->getID() == stopid;});
        if(pos != tram_shedule.cend()){
            return pos->time;
        }else{
            return TimeOfDay{.hour = -1, .minute = -1};
        }
    }

    int getID(const ::Ice::Current& current) {
        return this->tram_id;
    }

    schedule getSchedule(const ::Ice::Current& current) {
        return tram_shedule;
    }

    void registerUser(UserPrxPtr user, const ::Ice::Current& current) {
        registered_users.push_back(user);
    }

    void unregisterUser(UserPrxPtr user, const ::Ice::Current& current) {
        auto it = std::remove_if(registered_users.begin(), registered_users.end(), [&user](const auto& ptr) { return ptr == user; });
        if (it != registered_users.end())
        {
            registered_users.erase(it, registered_users.end());
        }
    }

    void createSchedule(){
        auto stops=currentLine->getStops();
       int offset=0;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(4, 20);

        for(auto stop:stops){
            offset += dis(gen);
            tram_shedule.push_back(ScheduleItem{.stop=stop,.time=createTIme(startTime,offset)});
        }

    }
    TimeOfDay createTIme(TimeOfDay curTime,int offset){
        TimeOfDay newTime;
        newTime.minute = curTime.minute + offset;
        newTime.hour = curTime.hour + newTime.minute / 60;
        newTime.minute = newTime.minute % 60;
        newTime.hour = newTime.hour % 24;
        return newTime;
    }

    void exitLine(){
        currentLine->removeTram(tram_proxy);
    }

    void start(){
        while (true){
            for(auto user:registered_users){
                try{
                user->updateStop(tram_proxy,tram_shedule[0].stop);
                }catch(const std::exception& e){

                }
            }
            cout<<"Arrived at stop "<<tram_shedule[0].stop->getName()<<endl;
            tram_shedule.erase(tram_shedule.cbegin());

            if(tram_shedule.empty()){
                break;
            }
            this_thread::sleep_for(10s);
        }
        currentLine->removeTram(tram_proxy);
    }
};

int main(int argc, char* argv[]){
    Ice::CtrlCHandler ctrlCHandler;
    try
    {

        Ice::CommunicatorHolder ich(argc, argv);
        ctrlCHandler.setCallback([&ich](int signal){
             ich.communicator()->destroy();
             quick_exit(0);
         });

        if(argc < 5){
            cout << "Incorrect invocation parameters SERVER_ADDR SERVER_PORT SERVER_NAME PORT TRAM_ID>" << endl;
            return 1;
        }

        int train_id;
        try{
            train_id = stoi(argv[5]);
        }catch (const std::invalid_argument& e) {
            cout << "Incorrect invocation parameters SERVER_ADDR SERVER_PORT SERVER_NAME PORT TRAM_ID>" << endl;
            return 1;
        } catch (const std::out_of_range& e) {
            cout << "Incorrect invocation parameters SERVER_ADDR SERVER_PORT SERVER_NAME PORT TRAM_ID>" << endl;
            return 1;
        }

        auto adapter = ich->createObjectAdapterWithEndpoints("", string("default -p ").append(argv[4]));
        auto sip_proxy = Ice::checkedCast<SIPPrx>(ich->stringToProxy(string(argv[3]).append(":default -h ").append(argv[1]).append(" -p ").append(argv[2])));
        auto lines = sip_proxy->getLines();
        unsigned line_inedx;
        TimeOfDay start_time;
        auto tram = make_shared<TramI>();
        while(true){
            int i=0;
            for(auto line :lines){
                cout<<i<<". ";
                for(auto stop:line->getStops()){
                    cout<<stop->getName()<<" ";
                }
                i++;
                cout<<endl;
            }
            cout<<"Select Line: ";
            cin>>line_inedx;
            if(cin.fail() || line_inedx < 0 || line_inedx>=lines.size()){
                cout << "Incorrect value" << endl;
                if(cin.fail()){
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                }
                continue;
            }
            break;
        }

        while (true){
            cout<<"Set Time :";
            int hours,minutes;
            cin>>hours>>minutes;
            if(cin.fail() || hours < 0 || hours>24 || minutes<0 || minutes>60){
                cout << "Incorrect value" << endl;
                if(cin.fail()){
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                }
                continue;
            }
            start_time.hour=hours;
            start_time.minute=minutes;
            break;
        }

        tram_proxy = Ice::checkedCast<TramPrx>(adapter->addWithUUID(tram));
        adapter->activate();
        auto current_line = lines[line_inedx];
        tram->startTram(train_id, current_line, start_time);
        tram->createSchedule();
        ctrlCHandler.setCallback([&ich, &tram](int signal){
            tram->exitLine();
            ich.communicator()->destroy();
            quick_exit(0);
        });
        cout<<"Route started"<<endl;
        tram->start();
        cout<<"Route done"<<endl;
        ich.communicator()->shutdown();
        quick_exit(0);
    }
    catch(const std::exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
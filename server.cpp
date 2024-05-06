#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <Ice/Initialize.h>
#include <thread>
#include <Ice/Proxy.h>
#include <algorithm>

using namespace std;
using namespace MPK;
 
int next_stop_id = 0;
SIPPrxPtr sip_proxy;

class LineI: public Line{

    stops line_stops;
    trams line_trams;

    public:

    LineI(vector<StopPrxPtr> stops) : line_stops(stops) {};

    trams getTrams(const ::Ice::Current& current) {
        return line_trams;
    }

    stops getStops(const ::Ice::Current& current) {
        return line_stops;
    }

    void addTram(TramPrxPtr tram, const ::Ice::Current& current) {
        line_trams.push_back(tram);
        cout << "New tram joined" << endl;
    }

    void removeTram(TramPrxPtr tram, const ::Ice::Current& current) {
        auto it = std::remove_if(line_trams.begin(), line_trams.end(), [&tram](TramPrxPtr s){return s->ice_getIdentity() == tram->ice_getIdentity();});
        if(it != line_trams.end()){
            line_trams.erase(it);
            cout<<"A tram has left"<<endl;
        }
    }



};

class StopI: public Stop
{
    string stop_name;
    int stop_id;
    vector<UserPrxPtr> stop_users;

    public:

    StopI(string name) : stop_name(name) {this->stop_id = next_stop_id++;};

    lines getLines(const ::Ice::Current& current){
        lines my_lines;
        for(auto line: sip_proxy->getLines()){
            auto line_stops = line->getStops();
            auto pos = find_if(line_stops.cbegin(), line_stops.cend(), [&current](StopPrxPtr s){return s->ice_getIdentity() == current.id;});
            if(pos != line_stops.cend()){
                my_lines.push_back(line);
            }        
        }
        return my_lines;
    }

    int getID(const ::Ice::Current& current){
        return stop_id;
    }

    ::std::string getName(const ::Ice::Current& current){
        return stop_name;
    }

    arrivals getArrivals(const ::Ice::Current& current){
        arrivals stop_arrivals;
        for(auto line : getLines(current)){
            for(auto tram: line->getTrams()){
                for(auto si: tram->getSchedule()){
                    if(si.stop->ice_getIdentity()==current.id){
                        stop_arrivals.push_back(ArrivalInfoItem{.tram=tram, .time=si.time});
                    }
                }
            }
        }
        return stop_arrivals;
    }

    void registerUser(UserPrxPtr user, const ::Ice::Current& current){
        cout<<"A user has joined"<<endl;
        stop_users.push_back(user);
    }

    void unregisterUser(UserPrxPtr user, const ::Ice::Current& current){
        auto it = std::remove_if(stop_users.begin(), stop_users.end(), [&user](UserPrxPtr ptr) { return ptr->ice_getIdentity() == user->ice_getIdentity();});
        if (it != stop_users.end())
        {
            stop_users.erase(it);
            cout<<"A user has left"<<endl;
        }
    }

    void updateUsers(StopPrxPtr stopPrx){
        for(auto user:stop_users){
            try{
                user->updateSchedule(stopPrx, stopPrx->getArrivals());
            }catch(const std::exception& e){
                continue;
            }
        }
    }

};

class SIPI :  public SIP
{
    lines sip_lines;
public:

    lines getLines(const ::Ice::Current& current) {
        return sip_lines;
    }

    void addLine(LinePrxPtr line, const ::Ice::Current& current) {
        sip_lines.push_back(line);
    }


    void removeLine(LinePrxPtr line, const ::Ice::Current& current) {
        auto it = std::remove_if(sip_lines.begin(), sip_lines.end(), [&line](const auto& ptr) { return ptr == line; });
        if (it != sip_lines.end())
        {
            sip_lines.erase(it, sip_lines.end());
        }
    }

    StopPrxPtr getStop(int ID, const ::Ice::Current& current) {
        for(auto line: sip_lines){
            for(auto stop: line->getStops()){
                if(stop->getID() == ID){
                    return stop;
                }
            }
        }
        return NULL;
    }

};
 
int
main(int argc, char* argv[])
{
    try
    {

        Ice::CommunicatorHolder ich(argc, argv);
        if(argc < 3){
            cout << "Incorrect invocation parameters SIP_PORT SIP_NAME" << endl;
            return 1;
        }
        auto adapter = ich->createObjectAdapterWithEndpoints("", string("default -p ").append(argv[1]));
        sip_proxy = Ice::checkedCast<SIPPrx>(adapter->add(make_shared<SIPI>(), Ice::stringToIdentity(argv[2])));
        auto stop_names = {"Mickiewicza", "Baldurskiego", "Lechonia", "Brzechwy", "Sienkiewicza", "Limanowskiego", "Polna", "Lesna", "Dluga"};
        vector<shared_ptr<StopI>> line_stops;
        for(auto stop_name: stop_names){
            line_stops.push_back(make_shared<StopI>(stop_name));
        }
        stops stop_proxies;
        for(auto stop: line_stops){
            auto stop_proxy = Ice::checkedCast<StopPrx>(adapter->addWithUUID(stop));
            stop_proxies.push_back(stop_proxy);
        }

        stops line1_stops;
        line1_stops.push_back(stop_proxies[0]);
        line1_stops.push_back(stop_proxies[1]);
        line1_stops.push_back(stop_proxies[2]);
        auto line1_proxy = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line1_stops)));
        sip_proxy->addLine(line1_proxy);

        stops line2_stops;
        line2_stops.push_back(stop_proxies[3]);
        line2_stops.push_back(stop_proxies[0]);
        line2_stops.push_back(stop_proxies[4]);
        auto line2_proxy = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line2_stops)));
        sip_proxy->addLine(line2_proxy);

        stops line3_stops;
        line3_stops.push_back(stop_proxies[5]);
        line3_stops.push_back(stop_proxies[2]);
        line3_stops.push_back(stop_proxies[6]);
        auto line3_proxies = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line3_stops)));
        sip_proxy->addLine(line3_proxies);

        adapter->activate();
        cout << "Connected to server" << endl;
        while (true){
            for(int iter=0;iter<line_stops.size();iter++){
                line_stops[iter]->updateUsers(stop_proxies[iter]);
            }
            this_thread::sleep_for(12s);
        }
    }
    catch(const std::exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
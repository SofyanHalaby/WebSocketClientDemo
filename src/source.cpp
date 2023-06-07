#include "websocketwrapper.hpp"

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <queue>
#include <mutex>
#include <iomanip>

//subscribe diff_order_book_bcheur

static const std::string URI ="wss://ws.bitstamp.net";

class ProducersConsumer
{
private:
    bool m_stop;
    std::queue<std::string> m_queue;
    std::mutex m_mtx;
    std::thread m_consumer_thread;
    std::ofstream& out;

    void Consume()
    {
        std::string data;
        while(!m_stop || m_queue.size() > 0)
        {
            while(m_queue.size()>1)
            {
                
                out << m_queue.front() <<std::endl;
                //std::cout<<"consuming ... " << m_queue.front() <<std::endl;
                m_queue.pop();
                
            }
            if(m_queue.size() == 1)
            {
                //std::cout<<"consuming ... " << m_queue.front() <<std::endl;
                m_mtx.lock();
                data = m_queue.front();
                m_queue.pop();
                m_mtx.unlock();

                out<<data<<std::endl;
                
            }
        }
    }

public:
    ProducersConsumer(std::ofstream& out):out(out), m_stop(false)
    {
        m_consumer_thread = std::thread(std::bind(&ProducersConsumer::Consume, this));
    }
    virtual ~ProducersConsumer()
    {
        m_consumer_thread.join();
    }
    void Produce(std::string str)
    {
        //std::cout<<"producing ... " << str <<std::endl;
        m_mtx.lock();
        m_queue.push(str);
        m_mtx.unlock();
    }
    void Stop()
    {
        m_stop = true;
    }
    
};


class Listener : public EventHandler
{
private:
    ProducersConsumer& m_logger;
    ProducersConsumer& m_message_recorder;

public:
    Listener(ProducersConsumer& message_recorder, ProducersConsumer& logger) 
                : m_message_recorder(message_recorder), m_logger(logger)
    {
    }
    void HandleNewMessage(std::string message)
    {
        m_message_recorder.Produce(message);
    }
    void HandleLogs(std::string log)
    {
        m_logger.Produce(log);
    }
    void Stop()
    {
        m_logger.Stop();
        m_message_recorder.Stop();
    }
};

class SubscribtionDetails : public Subscribtion
{
private:
    std::string m_channel;
public:
    SubscribtionDetails(std::string& channel):m_channel(channel)
    {
    }
    virtual std::string GenerateRequest() override
    {
        std::stringstream ss;
        ss << "{  \"event\": \"bts:subscribe\",  \"data\": {  \"channel\": \"" << m_channel << "\"  } }";
        return ss.str();
    }
    std::string GetChannel()
    {
        return m_channel;
    }
};

void LoadBulk(std::ifstream& input_file, WebSocketEndPoint& endpoint)
{
    std::string line;
    int line_idx = 0;
    // int num_of_connections = 0;
    while(std::getline(input_file, line))
    {
        ++line_idx;
        boost::trim(line);
        if(line.empty())
        {
            std::cout<<"[WARNING] cannot read data at line [" << line_idx << "], line skipped!" <<std::endl;
            continue;
        }

        int new_id = endpoint.Subscribe(std::shared_ptr<SubscribtionDetails>(new SubscribtionDetails(line)));
        if (new_id < 0)
        {
            std::cout << "[Error] cannot subscribe to channel " << line
                      << " please refer to log file for more details" << std::endl;
            continue;
        }
        std::cout << "[INFO] reader with id : [" << new_id << "] is subscribing channel: " << line << std::endl;
    }
}


int main(int argc, char *argv[])
{
    std::ofstream record_file;
    std::ofstream log_file;

    if(argc == 3)
    {
        record_file.open(argv[1]);
        if(!record_file.is_open()){
            std::cout<< "usage : app recordfile logfile " <<std::endl;
            std::cout<< "[ERROR] cannot open file at " << argv[1] <<std::endl;
            return 1; 
        }
        log_file.open(argv[2]);
        if(!log_file.is_open()){
            record_file.close();
            std::cout<< "usage : app recordfile logfile " <<std::endl;
            std::cout<< "[ERROR]cannot open file at " << argv[2] <<std::endl;
            return 1; 
        }
    }
    else
    {
        std::cout<< "usage : app recordfile logfile " <<std::endl;
        return 1; 
    }

    ProducersConsumer logger(log_file);
    ProducersConsumer message_recorder(record_file);

    std::shared_ptr<Listener> listener(new Listener( message_recorder,logger));
    WebSocketEndPoint endpoint(URI, listener);



    std::string input;
    bool done = false;
    std::cout << "WebSocket Connection to " << URI <<std::endl;
    
    while (!done)
    {
        std::cout<<">"; 
        std::getline(std::cin, input);
        boost::trim(input);
        if(input.empty()){
            continue;
        }
        std::stringstream ss(input);
        std::string cmd;
        ss>>cmd;

        if (cmd == "quit")
        {
            done = true;
            listener->Stop();
        }
        else if (cmd == "help")
        {
            std::cout
                 << "\nCommand List:\n"
                 << "subscribe <channel_name>\n"
                 << "disconnect <reader_idx>\n"
                << "bulk <file_name>\n"
                << "active: display details about active connections\n"
                << "help: Display this help text\n"
                << "quit: Exit the program\n"
                << std::endl;
        }
        else if(cmd =="subscribe")
        {
            //subscribe "channel"
            std::string channel;
            ss>> channel;
            if(ss.fail())
            {
                std::cout <<"[Error] bad arguments"<<std::endl;
                continue;
            }
            int new_id = endpoint.Subscribe(std::shared_ptr<SubscribtionDetails>(new SubscribtionDetails(channel)));
            if(new_id < 0)
            {
                std::cout<< "[ERROR] Connect initialization error: " <<std::endl;
                continue;
            }
            std::cout<<"[INFO]: connection created successfully with id["<< new_id <<"]" <<std::endl;
        }
        else if(cmd == "disconnect")
        {
            //disconnect idx
            int idx = -1;
            ss>>idx;
            if(ss.fail())
            {
                std::cout <<"[Error] bad arguments"<<std::endl;
                continue;
            }
            if(!endpoint.Disconnect(idx))
            {
                std::cout << "[Error] cannot disconnect from reader [" << idx
                <<"] please refer to log file for more details" << std::endl;
                continue;
            }
        }
        else if(cmd == "bulk")
        {
            //file "filepath"
            std::string file_path;
            ss>>file_path;
            
            if(ss.fail())
            {
                std::cout <<"[Error] bad arguments"<<std::endl;
                continue;
            }
            std::ifstream infile(file_path);
            //infile.open();
            std::cout<<"filepath: "<<file_path << "\t status: " << infile.is_open() <<std::endl;
            // if(!infile.is_open())
            // {
            //     infile.open(file_path);
            // }
            LoadBulk(infile, endpoint);
        }
        else if(input == "active")
        {
            //show active connections[id, channel]
            for(auto p : endpoint.GetConnections())
            {
                std::cout<< std::left << std::setw(10) << p.first
                    << std::static_pointer_cast<SubscribtionDetails>(p.second->GetSubscribtion())->GetChannel() <<std::endl;
            }

        }

        
    } 

    std::cout<<"ended! ..." <<std::endl;
    
    


    return 0;
}

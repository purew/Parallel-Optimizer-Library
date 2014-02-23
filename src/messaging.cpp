
#include "messaging.h"
#include <errno>
#include <iostream>

int Ventilator::send_task(int task_id, Parameters list)
{
    // Send data consisting of:
    //     int task_id, int nbr_params, double param0, double param1, etc
    int len = 2*sizeof(int) + sizeof(double)*list.size();
    char *data = new char[len];
    *int(data) = task_id;
    *int(data[sizeof(int)]) = len;
    int i=0;
    for (auto val : list) {
        int index = i*sizeof(double) + sizeof(int);
        *double(&(data[index])) = val;
        i += 1;
    }
    zmq_send(sock_mw, data, len);
    delete[] data;
}

int Ventilator::distribute_task(int task_id, Parameters params)
{
    buff_size = 0;
    char buffer[0]
    zmq_receive(sock_mw, buffer, buff_size);
    send_task(task_id, params);
}

task Ventilator::get_finished_task(bool blocking=true)
{
}

Ventilator::get_conn_type(int stage)
{
    if (stage==0)
        return ZMQ_REP;
    else
        return ZMQ_PULL;
}

MsgWorker::get_conn_type(int stage)
{
    if (stage==0)
        return ZMQ_REQ;
    else
        return ZMQ_PUSH;
}

BaseMessenger::connect(std::string endpoint, int port1=5555, int port2=5556)
{
    sock_mw = zmq_socket(context, get_conn_type(0));
    sock_wm = zmq_socket(context, get_conn_type(1));
    std::string url_mw = endpoint+std::to_string(port1);
    std::string url_wm = endpoint+std::to_string(port2);
    int rc1 = zmq_connect(sock_mw, url_mw.c_str());
    int rc2 = zmq_connect(sock_wm, url_wm.c_str());
    if (rc1 or rc2) {
        return -1;
    }
    else {
        is_connected = true;
        return 0;
    }
}

void destroy_messaging()
{
    zmq_ctx_destroy(context);
}

void* BaseMessenger::context = 0;

BaseMessenger::BaseMessenger()
    : is_connected(false)
{
    // Only initiate new zmq_context if none exists process-wide
    // Todo: Race-condition here if two threads run code at the same time
    if (context == 0)
        context = zmq_ctx_new();
}

BaseMessenger::~BaseMessenger()
{
    if (is_connected) {
        zmq_close(sock_in);
    }
}


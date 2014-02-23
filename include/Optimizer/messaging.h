
#ifndef MESSAGING_H_
#define MESSAGING_H_

#include <string>

extern "C" {
    #include <zmq.h>
}


// Forward declarations
class Parameters;
class BaseMessenger;

struct task_s {
    int task_id;
    Parameters params;
    double value;
};

/** Clean-up messaging. Call at end of program. */
void destroy_messaging();

/** Ventilator handles messaging between master-thread and workers.
    Main-thread should instantiate this class.*/
class Ventilator : public BaseMessenger
{
public:
    /** Distribute task to workers with a task_id.    */
    int distribute_task(task_s task);

    /** Ask for a finished task.    */
    task_s get_finished_task(bool blocking=true);
private:
    int Ventilator::send_task(int task_id, Parameters list)

};

/** MsgWorker handles messaging between workers and master.
    Worker-threads should instantiate this class.*/
class MsgWorker : public BaseMessenger
{
    /** Get task for processing    */
    task_s get_task();

    int report_task(task_s task);
};

class BaseMessenger
{
public:
    /** Sets up Ventilator as role, communicating through endpoint.
        \param endpoint localhost or ip-address
        \param port1 First port to use
        \param port2 Second port to use (both are needed)
        Returns 0 on success. At fail, sets errno.     */
    int connect(std::string endpoint, int port1=5555, int port2=5556);

    BaseMessenger();
    ~BaseMessenger();
protected:
    int port1, port2;
    void* get_conn_type()=0;
    bool is_connected;
    // zmq-objects
    static void *context;
    void *sock_mw; // stage 0, master->worker
    void *sock_wm;// stage 1, worker->master
};

#endif

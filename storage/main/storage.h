#ifndef STORAGE_H
#define STORAGE_H
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

bool init_SD();
void try_writing(File f);
void start_handler_task(QueueHandle_t requests, QueueHandle_t responses);

struct Record
{
    uint8_t tag;
    uint16_t data[3];
};

struct Open
{
    char filename[32];
};

struct Close
{
};

union MsgData
{
    struct Open open;
    struct Close close;
    struct Record record;
};

// We need response types to handle errors...
struct ResponseData
{
    bool success;
    char message[64];
};

enum class FileState
{
    Open,       // A file is open and being written to
    Closed,     // Any previous file has been successfully closed.
    FileError,  // An error has occurred, but may not be fatal.
    FatalError, // A fatal error has occurred, and no further writes are possible.
};

class FileHandler
{

public:
    FileHandler(size_t queue_length)
    {
        requests = xQueueCreate(queue_length, sizeof(MsgData));
        ticks_to_wait = 100; // 100 msec, since ticks are 1 msec.
    }

    bool send(const MsgData &data, TickType_t ticks_to_wait = portMAX_DELAY)
    {
        return xQueueSend(requests, &data, ticks_to_wait) == pdTRUE;
    }

    bool open(const char *filename)
    {
        MsgData data;
        strncpy(data.open.filename, filename, sizeof(data.open.filename));
        return send(data, ticks_to_wait);
    }

    bool close(TickType_t)
    {
        MsgData data;
        data.close = Close{};
        return send(data, ticks_to_wait);
    }

    bool record(Record record)
    {
        MsgData msg;
        msg.record = record;
        return send(msg, ticks_to_wait);
    }

private:
    QueueHandle_t requests;
    QueueHandle_t responses;
    TickType_t ticks_to_wait;
};

#endif // STORAGE_H

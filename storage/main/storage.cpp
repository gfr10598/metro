#include "storage.h"

bool init_SD()
{
    int SD_CS = GPIO_NUM_45;
    int SCK = 39;
    int MISO = 21;
    int MOSI = 42;

    SPI.begin(SCK, MISO, MOSI, SD_CS);
    SPI.setDataMode(SPI_MODE0);

    if (!SD.begin(SD_CS)) // GPIO45 is CS
    {
        printf("Card Mount Failed\n");
        return false;
    }

    return true;
}
uint8_t data[8192] = {0}; // Not on the stack!!

void try_writing(File f)
{
    auto start = millis();
    // The write takes 11 msec, but does not actually write until close()
    // It scales roughly with size.  8k takes 20 msec.
    f.write(&data[0], 4096);
    yield();
    printf("Wrote 4096 bytes in %lu ms\n", millis() - start);
}

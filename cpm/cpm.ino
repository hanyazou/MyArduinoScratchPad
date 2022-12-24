
#include <z80retroshield.h>
#include <SdFat.h>

// #define CPM_DEBUG
// #define CPM_DEBUG_DISKIO
#ifdef CPM_DEBUG
bool verbose_debug_instruction = false;
bool done = false;
static int stop_count = 0;
#endif


//
// Initial prgram loader.
//
unsigned char memory[1024 * 64] =
{
0xc3,0x96,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x42,0x4f,0x4f,0x54,0x3a,0x20,0x65,0x72,0x72,0x6f,0x72,0x20,0x62,0x6f,0x6f,0x74,
0x69,0x6e,0x67,0x0d,0x0a,0x00,0x01,0x01,0x00,0x21,0x00,0x00,0xaf,0xd3,0x0a,0x78,
0xd3,0x0b,0x79,0xd3,0x0c,0x7d,0xd3,0x0f,0x7c,0xd3,0x10,0xaf,0xd3,0x0d,0xdb,0x0e,
0xb7,0xca,0x00,0x00,0x21,0x80,0x00,0x7e,0xb7,0xca,0xc2,0x00,0xd3,0x01,0x23,0xc3,
0xb7,0x00,0xf3,0x76
};
int memory_len = sizeof(memory) / sizeof(memory[0]);

//
// Scratch area for format-strings.
//
char tmp[256] = { '\0' };

//
// Our helper
//
Z80RetroShield cpu;

void hex_dump(char* hdr, int addr, int len)
{
    while (0 < len) {
    sprintf(tmp,
        "%s%04x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        hdr, addr,
        memory[addr + 0], memory[addr + 1], memory[addr + 2], memory[addr + 3],
        memory[addr + 4], memory[addr + 5], memory[addr + 6], memory[addr + 7],
        memory[addr + 8], memory[addr + 9], memory[addr + 10], memory[addr + 11],
        memory[addr + 12], memory[addr + 13], memory[addr + 14], memory[addr + 15]);
        Serial.println(tmp);
        addr += 16;
        len -= 16;
    }
}

//
// RAM I/O function handler.
//
char memory_read(int address)
{
#ifdef CPM_DEBUG
    if (address == 0xfa00) {
        sprintf(tmp, "#### read 0xfa00");
        Serial.println(tmp);
        //stop_count = 1000;
        //verbose_debug_instruction = true;
        hex_dump("#### ", 0xfa00, 32);
    }
#endif

    if (address >= 0 && address < memory_len)
        return (memory[address]) ;
    else
    {
        sprintf(tmp, "Attempted read out of bounds memory %04x", address);
        Serial.println(tmp);
        return 0;
    }
}

//
// RAM I/O function handler.
//
void memory_write(int address, char value)
{
    if (address >= 0 && address < memory_len)
        memory[address] = value;
    else
    {
        sprintf(tmp, "Attempted write to %04x - %02d", address, value);
        Serial.println(tmp);
    }
}

static uint8_t io_fdc_drive = 0;
static uint8_t io_fdc_track = 0;
static uint8_t io_fdc_sector = 0;
static uint8_t io_fdc_status = 1;  // XXX, non zero means error
static uint16_t io_fdc_dma = 0;

static const int NumDrives = 16;
SdFs sd_card;
typedef struct {
    unsigned int sectors;
    FsFile image_file;
} IoDrive;
IoDrive io_drives[NumDrives] = {
    { 26 },
    { 26 },
    { 26 },
    { 26 },
    { },
    { },
    { },
    { },
    { 128 },
    { 128 },
    { 128 },
    { 128 },
    { },
    { },
    { },
    { 16484 },
};

//
// I/O function handler.
//
char io_read(int address)
{
    uint8_t ret = 0;

    switch (address) {
    case 0:  // console status port
        ret = 0;
        break;
    case 1:  // console data port
        // Read and return the character.
        while (! Serial.available())
            delay(1);
        ret = Serial.read();
        break;

    case 10:  // fdc-port: # of drive
        break;
    case 11:  // fdc-port: # of track
        break;
    case 12:  // fdc-port: # of sector
        break;
    case 13:  // fdc-port: command
        break;
    case 14:  // fdc-port: status
        ret = io_fdc_status;
        break;
    case 15:  // dma-port: dma address low
        ret = (io_fdc_dma & 0xff);
        break;
    case 16:  // dma-port: dma address high
        ret = ((io_fdc_dma >> 8) & 0xff);
        break;

    default:
        sprintf(tmp, "Attempted to read I/O port %d", address);
        Serial.println(tmp);
        break;
    }

    return (signed)ret;
}

//
// I/O function handler.
//
void io_write(int address, char val)
{
    uint8_t uval = (unsigned)val;

    switch (address) {
    case 0:  // console status port
	// just ignore
        break;
    case 1:  // console data port
        Serial.write(uval);
        break;

    case 10:  // fdc-port: # of drive
        io_fdc_drive = uval;
        break;
    case 11:  // fdc-port: # of track
        io_fdc_track = uval;
        break;
    case 12:  // fdc-port: # of sector
        io_fdc_sector = uval;
        break;
    case 13:  // fdc-port: command
        {
        auto& drive = io_drives[io_fdc_drive];
        auto& file = drive.image_file;
        io_fdc_status = 1;  // error
        if (uval != 0 && uval != 1) {
            sprintf(tmp, "Unknown FDC command: %02XH", uval);
            Serial.println(tmp);
            break;
        }
        uint32_t lba = ((uint32_t)io_fdc_track * drive.sectors + io_fdc_sector - 1);
        uint32_t offset = lba * 128;
        if (uval == 0) {
            if (uval < NumDrives && drive.image_file.isOpen()) {
                if (file.seekSet(offset) &&
                    file.read(&memory[io_fdc_dma], 128) == 128) {
                    io_fdc_status = 0;  // succeeded
                    delay(1);
                }
            }
        } else
        if (uval == 1) {
            // TODO
        }
#ifdef CPM_DEBUG_DISKIO
        sprintf(tmp, "%5s: drive %d, track %2d, sector %2d, LBA %4u@%u to %04XH: status %d",
                uval == 0 ? "Read" : "Write",
                io_fdc_drive, io_fdc_track, io_fdc_sector, lba, offset, io_fdc_dma,
                io_fdc_status);
        Serial.println(tmp);
        hex_dump("    ", io_fdc_dma, 32);
#endif
#ifdef CPM_DEBUG
        if (uval == 0 && io_fdc_track == 2 && io_fdc_sector == 8) {
            sprintf(tmp, "#### read track 2, sector 8");
            Serial.println(tmp);
            //stop_count = 1000;
            verbose_debug_instruction = true;
        }
#endif
        break;
        }
    case 14:  // fdc-port: status
        // just ignore
        break;
    case 15:  // dma-port: dma address low
        io_fdc_dma = ((io_fdc_dma & 0xff00) | (uval & 0xff));
        break;
    case 16:  // dma-port: dma address high
        io_fdc_dma = ((io_fdc_dma & 0x00ff) | (uval << 8));
        break;

    default:
        sprintf(tmp, "Attempted write to I/O port %02X with value %02X",
                address, uval);
        Serial.println(tmp);
        break;
    }
}


#ifdef Z80RetroShield_DEBUG
//
// Debug message handler.
//
void debug_output(const char* msg) {
    if (verbose_debug_instruction)
        Serial.println(msg);
}
#endif


//
// Setup routine: Called once.
//
void setup()
{
    Serial.begin(115200);
    while (!Serial);

    //
    // Setup callbacks for memory read/writes.
    //
    cpu.set_ram_read(memory_read);
    cpu.set_ram_write(memory_write);

    //
    // Now setup a callback to be executed every time an IN/OUT
    // instruction is encountered.
    //
    cpu.set_io_read(io_read);
    cpu.set_io_write(io_write);

#ifdef Z80RetroShield_DEBUG
    //
    // Enable debug output.
    //
    cpu.set_debug_output(debug_output);
#endif

    //
    // Open CP/M disk images.
    //
    Serial.print("Starting up SD Card...");
    if (!sd_card.begin(SDCARD_SS_PIN)) {
        Serial.println("No card found");
    } else {
        Serial.println("Card found");
        for (int drive = 0; drive < NumDrives; drive++) {
            sprintf(tmp, "z80retroshield/disks/drive%c.dsk", 'a' + drive);
            if (io_drives[drive].image_file.open(&sd_card, tmp, O_RDWR)) {
                sprintf(tmp, "Image file drive%c.dsk is assigned to drive %c", 'a' + drive,
                        'A' + drive);
                Serial.println(tmp);
            }
        }
    }
    Serial.println();

    //
    // Configured.
    //
    Serial.println("Z80 configured; launching program.");
}


//
// Loop function: Called forever.
//
void loop()
{
#ifdef CPM_DEBUG
    if (stop_count != 0) {
        if (--stop_count == 0) {
                sprintf(tmp, "stop.");
                Serial.println(tmp);
                done = true;
        }
    }
    if (done) return;
    static int cycles = 0;
    if (cycles > 500)
        return;
    cycles++;
#endif

    //
    // Step the CPU.
    //
    cpu.Tick(100);
}

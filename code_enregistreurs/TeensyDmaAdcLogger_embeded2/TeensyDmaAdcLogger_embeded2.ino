#include <TimeLib.h>
#include <Snooze.h>
#include "DMAChannel.h"//test
#include "SdFat.h"
#include "FreeStack.h"
#include "RingBuf.h"

SnoozeAlarm alarm;
SnoozeBlock config1(alarm);
bool sleepAtNight=true;
bool wasSleeping=false;
int night_start=22; //si tu met une heure apres miniut (ex:1) il faut que tu change le or en and a la ligne 214 et 217
int night_end=6;


IntervalTimer myTimer;
int ledStateNb = 0;

// 400 sector RingBuf - could be larger on Teensy 4.1.
const size_t RING_BUF_SIZE = 400*512;

// Preallocate 0.5GiB file.
const uint64_t PRE_ALLOCATE_SIZE = 8ULL << 26;//<<30

// Use FIFO SDIO.
#define SD_CONFIG SdioConfig(FIFO_SDIO)


DMAChannel dma(true);

SdFs sd;

FsFile file;
int file_start_hour;
unsigned char ucDataBlock[49] = {
  // Offset 0x00000000 to 0x00000048
  0x52, 0x49, 0x46, 0x46, 0x24, 0x90, 0xE1, 0x06, 0x57, 0x41, 0x56, 0x45,
  0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x40, 0x9C, 0x00, 0x00, 0x80, 0x38, 0x01, 0x00, 0x02, 0x00, 0x10, 0x00,
  0x64, 0x61, 0x74, 0x61, 0x4F, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,0x00
};


//------------------------------------------------------------------------------
// Ping-pong DMA buffer.
DMAMEM static uint16_t __attribute__((aligned(32))) dmaBuf[2][256];
size_t dmaCount;

// RingBuf for 512 byte sectors.
RingBuf<FsFile, RING_BUF_SIZE> rb;

// Shared between ISR and background.
volatile size_t maxBytesUsed;
char filename[50];
volatile bool overrun;
int minute_offset=0;

int now_hour=0;
//------------------------------------------------------------------------------
//ISR.


static void isr() {
  if (rb.bytesFreeIsr() >= 512 && !overrun) {
    rb.memcpyIn(dmaBuf[dmaCount & 1], 512);
    dmaCount++;
    if (rb.bytesUsed() > maxBytesUsed) {
      maxBytesUsed = rb.bytesUsed();
    }
  } else {
    overrun = true;
  }
  dma.clearComplete();
  dma.clearInterrupt();
#if defined(__IMXRT1062__)
  // Handle clear interrupt glitch in Teensy 4.x!
  asm("DSB");
#endif  // defined(__IMXRT1062__)
}
//------------------------------------------------------------------------------
#if defined(__IMXRT1062__) // Teensy 4.0
#define SOURCE_SADDR ADC1_R0
#define SOURCE_EVENT DMAMUX_SOURCE_ADC1
#else
#define SOURCE_SADDR ADC0_RA
#define SOURCE_EVENT DMAMUX_SOURCE_ADC0
#endif
//------------------------------------------------------------------------------
// Should replace ADC stuff with calls to Teensy ADC library.
// https://github.com/pedvide/ADC
void blink_error() {
      while(1){
        digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(150);               // wait for a second
        digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
        delay(150);  
    }
}

void blinkLED() {
  if (ledStateNb %30==0) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }
  ledStateNb+=1;
}


static void init(uint8_t pin) {
  uint32_t adch;
  uint32_t i, sum = 0;
  // Actually, do many normal reads, to start with a nice DC level
  for (i=0; i < 1024; i++) {
    sum += analogRead(pin);
  }

   ADC0_CFG1 =
      // Mode basse consomation d energie
      ADC_CFG1_ADLPC |
      // Clock divide select, 0=direct, 1=div2, 2=div4, 3=div8
      ADC_CFG1_ADIV(2) |
      // Sample time configuration, 0=Short, 1=Long
      // ADC_CFG1_ADLSMP |
      // Conversion mode, 0=8 bit, 1=12 bit, 2=10 bit, 3=16 bit
      ADC_CFG1_MODE(3) |
      // Input clock, 0=bus, 1=bus/2, 2=OSCERCLK, 3=async
      ADC_CFG1_ADICLK(2);

  ADC0_CFG2 = ADC_CFG2_MUXSEL | ADC_CFG2_ADLSTS(3);
  
  // save channel
  adch = ADC0_SC1A & 0x1F;
  // DMA enable
  ADC0_SC2 |= ADC_SC2_DMAEN;
  // Continuous conversion enable + average 4 sample 
  ADC0_SC3 = ADC_SC3_ADCO | ADC_SC3_AVGE | ADC_SC3_AVGS(0);
  // Start ADC
  ADC0_SC1A = adch;
  
  // set up a DMA channel to store the ADC data
  dma.attachInterrupt(isr);
  dma.begin();
  dma.source((volatile const signed short &)SOURCE_SADDR);
  dma.destinationBuffer((volatile uint16_t*)dmaBuf, sizeof(dmaBuf));
  dma.interruptAtHalf();
  dma.interruptAtCompletion();
  dma.triggerAtHardwareEvent(SOURCE_EVENT);
  dma.enable();
}
//------------------------------------------------------------------------------
void stopDma() {
#if defined(__IMXRT1062__) // Teensy 4.0
  ADC1_GC = 0;
#else  // defined(__IMXRT1062__)
  ADC0_SC3 = 0;
#endif  // defined(__IMXRT1062__)
  dma.disable();
}
//------------------------------------------------------------------------------

void createFile() {
   do{
    snprintf(filename, 50, "audio-m%02d-j%02d_%02dh%02d.bin", month(), day(), hour(), (minute()+minute_offset) );
    minute_offset+=1;
  } while(sd.exists(filename));
  minute_offset=0;
  
  if (!file.open(filename, O_CREAT | O_TRUNC | O_RDWR)) {
    sd.errorHalt("file.open failed");
  }
    if (!file.preAllocate(PRE_ALLOCATE_SIZE)) {
    sd.errorHalt("file.preAllocate failed");
  }
    rb.begin(&file);
   for(int i=0;i<=49;i++){
    file.write(ucDataBlock[i]);
  }
  file_start_hour=hour(); //file_start_hour=hour();
}

void closeFile() {
  
  stopDma();
  if (!file.truncate()) {
    sd.errorHalt("truncate failed");
  }
  file.close();

}
//------------------------------------------------------------------------------
void runTest(uint8_t pin) {
  dmaCount = 0;
  maxBytesUsed = 0;
  overrun = false;

  
  createFile();
  init(pin);
  
  uint32_t samplingTime = micros();
  while (!overrun ) {
    size_t n = rb.bytesUsed();
    if ((n + file.curPosition()) >= (PRE_ALLOCATE_SIZE - 512)) {
      break;
    }
    if (n >= 512) {
      if (rb.writeOut(512) != 512) {
        file.close();
        return;
      }
    }
    now_hour=hour();
    if(sleepAtNight and (now_hour>=night_start or now_hour<=night_end)){
      closeFile();
      wasSleeping=true;
      while( now_hour>=night_start or now_hour<=night_end) {
        adjustTime(1740);
        Snooze.deepSleep(config1);
        now_hour=hour();
      }
    }
    if(file_start_hour != now_hour){   //if(file_start_hour != hour()){
      if( wasSleeping==false) {closeFile();}
      
      else {wasSleeping=false;}      
      
      createFile();
      init(pin);
    }
  }
  stopDma();
  samplingTime = (micros() - samplingTime);
  if (!file.truncate()) {
    sd.errorHalt("truncate failed");
  }
  file.close();
}

//------------------------------------------------------------------------------
void processSyncMessage() {
  
  unsigned long pctime = 1652773118;
  setTime(pctime+7200); // Sync Arduino clock to the time received on the serial port
 
}

void setup() {
  processSyncMessage();
  //setSyncProvider( requestSync); 
  pinMode(13, OUTPUT);
  myTimer.begin(blinkLED, 500000);

  alarm.setRtcTimer(0,29,0);//ne pas oublier de changer a valeur a la ligne 218
}
//------------------------------------------------------------------------------
void loop() {
  if (!sd.begin(SD_CONFIG)) {
    blink_error();
  }

  digitalWrite(13, HIGH);
  runTest(A0);
  blink_error();
}

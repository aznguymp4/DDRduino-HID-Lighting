#include "PluggableUSB.h"
#include "HID.h"
#include <FastLED.h>

typedef struct {
  uint8_t brightness;
} SingleLED;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGBLed;

typedef struct {
  uint8_t start; // starts at index 1 for simplicity lol
  uint8_t end;
} RGBPortion;

typedef struct {
  int inputPin;
  int multiplier;
  RGBPortion portion;
  int frq; // vvvv   For efficiency, hard code the values
  int numLeds; // (end-start)+1
  int halfLidx; // ((numLeds/2)-1)+start
  int halfRidx; // halfLidx + 1
} AudioVisualizerStrip;

// ******************************
// EDIT THESE LINES

// The single LEDs will be first in BTools
// The RGB LEDs will come afterwards, with R/G/B individually
#define NUMBER_OF_SINGLE 1
#define NUMBER_OF_RGB 8 // portions

#define NUM_LEDS 300
#define NEOPIXEL_PIN 12
#define NEOPIXEL_BRIGHTNESS 30

CRGB led[NUM_LEDS];

const int my_lights[NUMBER_OF_SINGLE] = {
  8,
};
const RGBPortion portions[NUMBER_OF_RGB] = {
  {start:1,end:3},
  {start:4,end:6},
  {start:7,end:8},
  {start:10,end:11},
  {start:20,end:24},
  {start:25,end:29},
  {start:30,end:34},
  {start:35,end:39},
};

AudioVisualizerStrip visualizers[2] {
  {
    inputPin: 4,
    multiplier: 3,
    portion: {start: 40, end: 79},
    frq: 0,
    numLeds: 40,
    halfLidx: 59,
    halfRidx: 60
  },
  {
    inputPin: 5,
    multiplier: 3,
    portion: {start: 100, end: 139},
    frq: 0,
    numLeds: 40,
    halfLidx: 119,
    halfRidx: 120
  }
};

void light_update(SingleLED* single_leds, RGBLed* rgb_leds) {
  for(int i=0;i<NUMBER_OF_SINGLE;i++) {
    analogWrite(my_lights[i], single_leds[i].brightness);
  }
  for(int i=0;i<NUMBER_OF_RGB;i++) {
    RGBPortion portion = portions[i];
    for(int j = portion.start-1; j < portion.end; j++) {
      led[j] = CRGB(rgb_leds[i].r, rgb_leds[i].g, rgb_leds[i].b);
    }
  }
  // needs_update = true;
}

void setup() {
  FastLED.addLeds<NEOPIXEL, NEOPIXEL_PIN>(led, NUM_LEDS);
  FastLED.setBrightness(NEOPIXEL_BRIGHTNESS);
  for(int i=0;i<2;i++) {
    pinMode(visualizers[i].inputPin, INPUT);
  }
}

void loop() {
  for(int i=0;i<2;i++) {
    AudioVisualizerStrip strip = visualizers[i];
    // Uncomment these lines if you don't want to hard code the values and make it less efficient lol
    // strip.numLeds = (strip.portion.end - strip.portion.start) + 1;
    // strip.halfLidx = int((strip.numLeds*.5)-1) + strip.portion.start;
    // strip.halfRidx = int(strip.numLeds*.5) + strip.portion.start;
    strip.frq = analogRead(strip.inputPin)*strip.multiplier;
    CHSV col = CHSV(strip.frq%160, 255, min(strip.frq, 255));
    led[strip.halfLidx] = col;
    led[strip.halfRidx] = col;
    for(int j=strip.portion.start; j<=strip.halfLidx-1; j++) {
      const int idx1 = strip.numLeds-j+(strip.portion.start*2)-1;
      led[j] = led[j+1];
      led[idx1] = led[idx1-1];
    }
  }
  delay(17);
  FastLED.show();
}

// ******************************
// don't need to edit below here

#define NUMBER_OF_LIGHTS (NUMBER_OF_SINGLE + NUMBER_OF_RGB*3)
#if NUMBER_OF_LIGHTS > 63
  #error You must have less than 64 lights
#endif

union {
  struct {
    SingleLED singles[NUMBER_OF_SINGLE];
    RGBLed rgb[NUMBER_OF_RGB];
  } leds;
  uint8_t raw[NUMBER_OF_LIGHTS];
} led_data;

static const uint8_t PROGMEM _hidReportLEDs[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x00,                    // USAGE (Undefined)
    0xa1, 0x01,                    // COLLECTION (Application)
    // Globals
    0x95, NUMBER_OF_LIGHTS,        //   REPORT_COUNT
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x05, 0x0a,                    //   USAGE_PAGE (Ordinals)
    // Locals
    0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
    0x29, NUMBER_OF_LIGHTS,        //   USAGE_MAXIMUM (Instance n)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    // BTools needs at least 1 input to work properly
    0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
    0x29, 0x01,                    //   USAGE_MAXIMUM (Instance 1)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0xc0                           // END_COLLECTION
};

// This is almost entirely copied from NicoHood's wonderful RawHID example
// Trimmed to the bare minimum
// https://github.com/NicoHood/HID/blob/master/src/SingleReport/RawHID.cpp
class HIDLED_ : public PluggableUSBModule {

  uint8_t epType[1];
  
  public:
    HIDLED_(void) : PluggableUSBModule(1, 1, epType) {
      epType[0] = EP_TYPE_INTERRUPT_IN;
      PluggableUSB().plug(this);
    }

    int getInterface(uint8_t* interfaceCount) {
      *interfaceCount += 1; // uses 1
      HIDDescriptor hidInterface = {
        D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
        D_HIDREPORT(sizeof(_hidReportLEDs)),
        D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 16)
      };
      return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
    }
    
    int getDescriptor(USBSetup& setup)
    {
      // Check if this is a HID Class Descriptor request
      if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) { return 0; }
      if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) { return 0; }
    
      // In a HID Class Descriptor wIndex contains the interface number
      if (setup.wIndex != pluggedInterface) { return 0; }
    
      return USB_SendControl(TRANSFER_PGM, _hidReportLEDs, sizeof(_hidReportLEDs));
    }
    
    bool setup(USBSetup& setup)
    {
      if (pluggedInterface != setup.wIndex) {
        return false;
      }
    
      uint8_t request = setup.bRequest;
      uint8_t requestType = setup.bmRequestType;
    
      if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
      {
        return true;
      }
    
      if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
        if (request == HID_SET_REPORT) {
          if(setup.wValueH == HID_REPORT_TYPE_OUTPUT && setup.wLength == NUMBER_OF_LIGHTS){
            USB_RecvControl(led_data.raw, NUMBER_OF_LIGHTS);
            light_update(led_data.leds.singles, led_data.leds.rgb);
            return true;
          }
        }
      }
    
      return false;
    }
};

HIDLED_ HIDLeds;
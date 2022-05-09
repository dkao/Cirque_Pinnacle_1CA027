// Copyright (c) 2018 Cirque Corp. Restrictions apply. See: www.cirque.com/sw-license

#include <SPI.h>
#define MOUSE_ENABLE
#ifdef MOUSE_ENABLE
  #include <Mouse.h>
  #define MOUSE_BEGIN         Mouse.begin()
  #define MOUSE_PRESS(x)      Mouse.press(x)
  #define MOUSE_RELEASE(x)    Mouse.release(x)
  #define MOUSE_CLICK(x)      Mouse.click(x)
  #define MOUSE_MOVE(x, y)    Mouse.move(x, y)
  #define MOUSE_SCROLL(v, h)  Mouse.scroll(v, h)
#else
  #define MOUSE_BEGIN         {}
  #define MOUSE_PRESS(x)      {}
  #define MOUSE_RELEASE(x)    {}
  #define MOUSE_CLICK(x)      {}
  #define MOUSE_MOVE(x, y)    {}
  #define MOUSE_SCROLL(v, h)  {}
#endif
#define SCROLL_ENABLE
#define GLIDE_ENABLE

// ___ Using a Cirque TM0XX0XX w/ Curved Overlay and Arduino ___
// This demonstration application is built to work with a Teensy 3.1/3.2 but it can easily be adapted to
// work with Arduino-based systems.
// When using with DK000013 development kit, connect sensor to the FFC connector
// labeled 'Sensor0'.
// This application connects to a TM0XX0XX circular touch pad via SPI. To verify that your touch pad is configured
// for SPI-mode, make sure that R1 is populated with a 470k resistor (or whichever resistor connects pins 24 & 25 of the 1CA027 IC).
// The pad is configured for Absolute mode tracking.  Touch data is sent in text format over USB CDC to
// the host PC.  You can open a terminal window on the PC to the USB CDC port and see X, Y, and Z data
// fill the window when you touch the sensor. Tools->Serial Monitor can be used to view touch data.
// NOTE: all config values applied in this sample are meant for a module using REXT = 976kOhm

//  Pinnacle TM0XX0XX with Arduino
//  Hardware Interface
//  GND
//  +3.3V
//  SCK = Pin 13
//  MISO = Pin 12
//  MOSI = Pin 11
//  SS = Pin 8
//  DR = Pin 7

// Hardware pin-number labels
#define SCK_PIN   13
#define DIN_PIN   12
#define DOUT_PIN  11
#define CS_PIN    10
#define DR_PIN    9

#define SDA_PIN   18
#define SCL_PIN   19

#define LED_0     21
#define LED_1     20

// Masks for Cirque Register Access Protocol (RAP)
#define WRITE_MASK  0x80
#define READ_MASK   0xA0

// Register config values for this demo
#define SYSCONFIG_1   0x00
#define FEEDCONFIG_1  0x03
#define FEEDCONFIG_2  0x1F
#define FEEDCONFIG_3  0x02
#define Z_IDLE_COUNT  0x05

// Coordinate scaling values
#define PINNACLE_XMAX     2047    // max value Pinnacle can report for X
#define PINNACLE_YMAX     1535    // max value Pinnacle can report for Y
#define PINNACLE_X_LOWER  127     // min "reachable" X value
#define PINNACLE_X_UPPER  1919    // max "reachable" X value
#define PINNACLE_Y_LOWER  63      // min "reachable" Y value
#define PINNACLE_Y_UPPER  1471    // max "reachable" Y value
#define PINNACLE_X_RANGE  (PINNACLE_X_UPPER-PINNACLE_X_LOWER)
#define PINNACLE_Y_RANGE  (PINNACLE_Y_UPPER-PINNACLE_Y_LOWER)
#define ZONESCALE 256   // divisor for reducing x,y values to an array index for the LUT
#define ROWS_Y ((PINNACLE_YMAX + 1) / ZONESCALE)
#define COLS_X ((PINNACLE_XMAX + 1) / ZONESCALE)

// ADC-attenuation settings (held in BIT_7 and BIT_6)
// 1X = most sensitive, 4X = least sensitive
#define ADC_ATTENUATE_1X   0x00
#define ADC_ATTENUATE_2X   0x40
#define ADC_ATTENUATE_3X   0x80
#define ADC_ATTENUATE_4X   0xC0

// Convenient way to store and access measurements
typedef struct _absData
{
  uint16_t xValue;
  uint16_t yValue;
  uint16_t zValue;
  uint8_t buttonFlags;
  bool touchDown;
  bool hovering;
} absData_t;

absData_t touchData;

//const uint16_t ZONESCALE = 256;
//const uint16_t ROWS_Y = 6;
//const uint16_t COLS_X = 8;

// These values require tuning for optimal touch-response
// Each element represents the Z-value below which is considered "hovering" in that XY region of the sensor.
// The values present are not guaranteed to work for all HW configurations.
const uint8_t ZVALUE_MAP[ROWS_Y][COLS_X] =
{
  {0, 0,  0,  0,  0,  0, 0, 0},
  {0, 2,  3,  5,  5,  3, 2, 0},
  {0, 3,  5, 15, 15,  5, 2, 0},
  {0, 3,  5, 15, 15,  5, 3, 0},
  {0, 2,  3,  5,  5,  3, 2, 0},
  {0, 0,  0,  0,  0,  0, 0, 0},
};

// setup() gets called once at power-up, sets up serial debug output and Cirque's Pinnacle ASIC.
void setup()
{
  Serial.begin(115200);
  while(!Serial); // needed for USB

  pinMode(LED_0, OUTPUT);

  Pinnacle_Init();

  // These functions are required for use with thick overlays (curved)
  setAdcAttenuation(ADC_ATTENUATE_2X);
  tuneEdgeSensitivity();

  Serial.println();
  // X: scaled X-axis absolute coordinate, with origin at center of the trackpad
  // Y: scaled Y-axis absolute coordinate, with origin at center of the trackpad
  // Z: touch energy(?), unchanged from raw report
  // Btn: button flags, unchanged from raw report
  // Dist: distance from origin of current coordinate, hypot(X, Y)
  // dX: X-axis coordinate change from previous point
  // dY: Y-axis coordinate change from previous point
  // Speed: distance between current coordinate and previous coordinate, hypot(dX, dY) / 1, unit time assumed to be constant between reports
  // Status:
  //     liftoff: Z-idle reported from controller, no touch presence detected
  //     hover: Z not strong enough to be recognized as a valid touch on surface, for information only at the moment, treated as a normal touch by mouse code
  //     valid: Z strong enough to be recognized as valid touch
  //     glide: fake inertial touch reports generated based on final velocity after liftoff
  //     scroll_detect: touch in outer ring, deciding whether to trigger scroll functionality or report as mouse movement
  //     vertical_scroll: touch originating in right half of trackpad, validated as scroll gesture
  //     horizontal_scroll: touch originating in left half of trackpad, validated as scroll gesture
  //     unknown: I missed a corner case somewhere, hopefully this never happens
  Serial.println("X\tY\tZ\tBtn\tDist\tdX\tdY\tSpeed\tStatus");
  MOUSE_BEGIN;
  Pinnacle_EnableFeed(true);
}

unsigned long current_time;
unsigned long last_report_time;
unsigned long last_status_check_time;
bool tracking = false;
bool scrolling_vert = false;
bool scrolling_hori = false;
bool scrolling_detect = false;
float x_prev, y_prev;
float x_delta, y_delta;
float x_scaled, y_scaled;
float magnitude, delta_magnitude;
unsigned long deceleration_counter;
#define BTN_COUNT 3
bool btns[BTN_COUNT];
uint8_t btn_buffers[BTN_COUNT];
char btn_codes[3] = {MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_LEFT};

#define STATUS_POLL_INTERVAL_US 1000
#define GLIDING_REPORT_INTERVAL_US 10000
#define HARDWARE_DATA_READY
//#define SOFTWARE_DATA_READY

// loop() continuously checks to see if data-ready (DR) is high. If so, reads and reports touch data to terminal.
void loop()
{
  const char *status_string = "unknown";
  current_time = micros();
  unsigned long elapsed = current_time - last_report_time;
#ifdef HARDWARE_DATA_READY
  if (DR_Asserted())
  {
#else
  // Poll status register for software data ready bit
  uint8_t status_reg;
  if ((current_time - last_status_check_time) > STATUS_POLL_INTERVAL_US)
  {
    RAP_ReadBytes(0x02, &status_reg, 1);
    last_status_check_time = current_time;
    if (!(status_reg & (1 << 2)))
    {
      goto exit;
    }
#endif
    Pinnacle_GetAbsolute(&touchData);
    Pinnacle_CheckValidTouch(&touchData); // Checks for "hover" caused by curved overlays

    //    ScaleData(&touchData, 1024, 1024);      // Scale coordinates to arbitrary X, Y resolution

    if (Pinnacle_zIdlePacket(&touchData))
    {
      x_scaled = 0;
      y_scaled = 0;
      if (tracking)
      {
        tracking = false;
        scrolling_vert = false;
        scrolling_hori = false;
        scrolling_detect = false;
        x_prev = 0;
        y_prev = 0;
      }
      status_string = "liftoff";
    }
    else
    {
      // Center origin and scale coordinates for vector calculations
#define TARGET_RESOLUTION 512
      x_scaled = (touchData.xValue - (PINNACLE_X_UPPER + PINNACLE_X_LOWER) / 2) * TARGET_RESOLUTION / PINNACLE_X_RANGE;
      y_scaled = (touchData.yValue - (PINNACLE_Y_UPPER + PINNACLE_Y_LOWER) / 2) * TARGET_RESOLUTION / PINNACLE_Y_RANGE;
      magnitude = hypot(x_scaled, y_scaled);
      if (!tracking)
      {
        tracking = true;
        // initialize tracking state
        x_prev = x_scaled;
        y_prev = y_scaled;
#ifdef SCROLL_ENABLE
#define OUTER_RING_WIDTH 80
#define OUTER_RING_THRESHOLD ((TARGET_RESOLUTION / 2) - OUTER_RING_WIDTH)
#define HORIZONTAL_AXIS -y_scaled
        if (magnitude >= OUTER_RING_THRESHOLD)
        {
          if (HORIZONTAL_AXIS >= 0)
          {
            scrolling_vert = true;
          }
          else
          {
            scrolling_hori = true;
          }
          scrolling_detect = true;
        }
#endif // SCROLL_ENABLE
      }
      x_delta = x_scaled - x_prev;
      y_delta = y_scaled - y_prev;
      delta_magnitude = hypot(x_delta, y_delta);
#ifdef SCROLL_ENABLE
      if (scrolling_vert || scrolling_hori || scrolling_detect)
      {
#define WHEEL_CLICKS_PER_ROTATION 36
        float dot = x_scaled * x_prev + y_scaled * y_prev;
        float det = x_prev * y_scaled - y_prev * x_scaled;
        float ang = atan2(det, dot);
        float scroll_clicks = ang * WHEEL_CLICKS_PER_ROTATION / 2 / M_PI;
        if (scrolling_vert) {
          status_string = "vertical_scroll";
        } else if (scrolling_hori) {
          status_string = "horizontal_scroll";
        }
        if (scrolling_detect)
        {
#define SCROLLING_MOVEMENT_THRESHOLD 10.0
#define SCROLLING_MOVEMENT_RATIO 2.0
          float magnitude_prev = hypot(x_prev, y_prev);
          float scalar_projection = dot / magnitude_prev;
          float scalar_rejection = det / magnitude_prev;
          float parallel_movement = abs(magnitude_prev - abs(scalar_projection));
          float perpendicular_movement = abs(scalar_rejection);
          if (delta_magnitude >= SCROLLING_MOVEMENT_THRESHOLD)
          {
            // scroll = perpendicular_movement / parallel_movement >= SCROLLING_MOVEMENT_RATIO
            if (parallel_movement * SCROLLING_MOVEMENT_RATIO > perpendicular_movement)
            {
              // Scroll criteria failed, abandon
              scrolling_vert = false;
              scrolling_hori = false;
            }
            // Scroll detection complete
            scrolling_detect = false;
          }
          status_string = "scroll_detect";
        }
        if (abs(scroll_clicks) >= 1 && (scrolling_vert || scrolling_hori) && !scrolling_detect)
        {
          if (scrolling_vert)
          {
            MOUSE_SCROLL(-scroll_clicks, 0);
            status_string = "vertical_scroll_event";
          }
          else
          {
            MOUSE_SCROLL(0, scroll_clicks);
            status_string = "horizontal_scroll_event";
          }
          // Distance to next click will be reset, not noticeable enough to be an issue
          x_prev = x_scaled;
          y_prev = y_scaled;
          last_report_time = current_time;
        }
        // Zero out deltas to disable glide on scroll lift
        x_delta = 0;
        y_delta = 0;
      }
      else
#endif // SCROLL_ENABLE
      {
        // TODO: Consolidate rotation macros into a single control
#define MOUSE_REPORT_X -y_delta
#define MOUSE_REPORT_Y x_delta
        MOUSE_MOVE(MOUSE_REPORT_X, MOUSE_REPORT_Y);
        x_prev = x_scaled;
        y_prev = y_scaled;
        last_report_time = current_time;
        if (touchData.hovering) {
          status_string = "hovering";
        } else {
          status_string = "valid";
        }
      }
      deceleration_counter = 0;
    }
    for (unsigned i = 0; i < BTN_COUNT; i++)
    {
      uint8_t btn_state = ((touchData.buttonFlags >> i) & 0x1);
      if (btn_state && !btns[i])
      {
        MOUSE_PRESS(btn_codes[i]);
        btns[i] = true;
      }
      else if (!btn_state && btns[i])
      {
        MOUSE_RELEASE(btn_codes[i]);
        btns[i] = false;
      }
    }
    // Debug prints
    Serial.print(x_scaled); // X
    Serial.print('\t');
    Serial.print(y_scaled); // Y
    Serial.print('\t');
    Serial.print(touchData.zValue); // Z
    Serial.print('\t');
    Serial.print(touchData.buttonFlags); // Btn
    Serial.print('\t');
    Serial.print(magnitude); // Dist
    Serial.print('\t');
    Serial.print(x_delta); // dX
    Serial.print('\t');
    Serial.print(y_delta); // dy
    Serial.print('\t');
    Serial.print(delta_magnitude); // Speed
    Serial.print('\t');
    Serial.println(status_string); // Status
  }
#ifdef GLIDE_ENABLE
  // Generate inertial cursor reports
  // See "GlideCursor: Pointing with an Inertial Cursor" (https://hal.archives-ouvertes.fr/hal-00989252)
  // Kinetic friction feels closer to an actual trackball, at least according to a Ploopy Classic w/ BTU evtest log I captured
  // While there is some curvature to the velocity vs. time plot, it's closer to linear than exponential
//#define USE_FLUID_FRICTION
#define USE_KINETIC_FRICTION
  else if (!tracking && elapsed >= GLIDING_REPORT_INTERVAL_US && (y_delta != 0 || x_delta != 0))
  {
    deceleration_counter++;
    // Compute new positions using friction model instead of new velocities
    // This avoids accumulating round-off errors as velocities get smaller
#ifdef USE_FLUID_FRICTION
#define FLUID_FRICTIOIN_COEF 0.03 // Some hand-tuned value in arbitrary units
    float xnew = (x_delta * (1 - exp(-DECELERATION_COEF * deceleration_counter)) / FLUID_FRICTION_COEF);
    float ynew = (y_delta * (1 - exp(-DECELERATION_COEF * deceleration_counter)) / FLUID_FRICTION_COEF);
#else // USE_KINETIC_FRICTION
#define KINETIC_FRICTION_COEF 0.4 // Some hand-tuned value in arbitrary units
    float v0 = delta_magnitude;
    float p = v0 * deceleration_counter - KINETIC_FRICTION_COEF * deceleration_counter * deceleration_counter / 2;
    float xnew = p * x_delta / v0;
    float ynew = p * y_delta / v0;
#endif // USE_FLUID_FRICTION
    // Note x_prev and y_prev are (0, 0) immediately after liftoff
    // Since we're only outputting deltas, it's OK to recenter the new positions around (0, 0)
    // But there will be discontinuations for the debug print
    float x_delta_new = (int)xnew - (int)x_prev;
    float y_delta_new = (int)ynew - (int)y_prev;
    x_prev = xnew;
    y_prev = ynew;
#define MOUSE_REPORT_X_GLIDE -y_delta_new
#define MOUSE_REPORT_Y_GLIDE x_delta_new
    MOUSE_MOVE(MOUSE_REPORT_X_GLIDE, MOUSE_REPORT_Y_GLIDE);

    // Debug prints
    Serial.print(xnew); // X
    Serial.print('\t');
    Serial.print(ynew); // Y
    Serial.print('\t');
    Serial.print('0'); // Z
    Serial.print('\t');
    uint8_t button_flag = 0;
    for (unsigned i = 0; i < BTN_COUNT; i++) {
      button_flag |= (!!btns[i] << i);
    }
    Serial.print(button_flag); // Btn
    Serial.print('\t');
    Serial.print(hypot(xnew, ynew)); // Dist
    Serial.print('\t');
    Serial.print(y_delta_new); // dX
    Serial.print('\t');
    Serial.print(x_delta_new); // dY
    Serial.print('\t');
    Serial.print(hypot(x_delta_new, y_delta_new)); // Speed
    Serial.print('\t');
    Serial.println("glide"); // Status

    // Stop gliding once speed is low enough
    if (abs(x_delta_new) <= 1 && abs(y_delta_new) <= 1)
    {
      x_delta = 0;
      y_delta = 0;
    }
    last_report_time = current_time;
  }
#endif // GLIDE_ENABLE
exit:
  AssertSensorLED(touchData.touchDown);
}

/*  Pinnacle-based TM0XX0XX Functions  */
void Pinnacle_Init()
{
  RAP_Init();
  DeAssert_CS();
  pinMode(DR_PIN, INPUT);

  // Host clears SW_CC flag
  Pinnacle_ClearFlags();

  // Host configures bits of registers 0x03 and 0x05
  RAP_Write(0x03, SYSCONFIG_1);
  RAP_Write(0x05, FEEDCONFIG_2);

  // Disable smoothing
  RAP_Write(0x06, FEEDCONFIG_3);

  // Host enables preferred output mode (absolute)
  RAP_Write(0x04, FEEDCONFIG_1);

  // Host sets z-idle packet count to 5 (default is 30)
  RAP_Write(0x0A, Z_IDLE_COUNT);
  Serial.println("Pinnacle Initialized...");
}

// Reads XYZ data from Pinnacle registers 0x14 through 0x17
// Stores result in absData_t struct with xValue, yValue, and zValue members
void Pinnacle_GetAbsolute(absData_t * result)
{
  uint8_t data[6] = { 0,0,0,0,0,0 };
  RAP_ReadBytes(0x12, data, 6);

  Pinnacle_ClearFlags();

  result->buttonFlags = data[0] & 0x3F;
  result->xValue = data[2] | ((data[4] & 0x0F) << 8);
  result->yValue = data[3] | ((data[4] & 0xF0) << 4);
  result->zValue = data[5] & 0x3F;

  result->touchDown = result->xValue != 0;
}

// Checks touch data to see if it is a z-idle packet (all zeros)
bool Pinnacle_zIdlePacket(absData_t * data)
{
  return data->xValue == 0 && data->yValue == 0 && data->zValue == 0;
}

// Clears Status1 register flags (SW_CC and SW_DR)
void Pinnacle_ClearFlags()
{
  RAP_Write(0x02, 0x00);
  delayMicroseconds(50);
}

// Enables/Disables the feed
void Pinnacle_EnableFeed(bool feedEnable)
{
  uint8_t temp;

  RAP_ReadBytes(0x04, &temp, 1);  // Store contents of FeedConfig1 register

  if(feedEnable)
  {
    temp |= 0x01;                 // Set Feed Enable bit
    RAP_Write(0x04, temp);
  }
  else
  {
    temp &= ~0x01;                // Clear Feed Enable bit
    RAP_Write(0x04, temp);
  }
}


/*  Curved Overlay Functions  */
// Adjusts the feedback in the ADC, effectively attenuating the finger signal
// By default, the the signal is maximally attenuated (ADC_ATTENUATE_4X for use with thin, flat overlays
void setAdcAttenuation(uint8_t adcGain)
{
  uint8_t temp = 0x00;

  Serial.println();
  Serial.println("Setting ADC gain...");
  ERA_ReadBytes(0x0187, &temp, 1);
  temp &= 0x3F; // clear top two bits
  temp |= adcGain;
  ERA_WriteByte(0x0187, temp);
  ERA_ReadBytes(0x0187, &temp, 1);
  Serial.print("ADC gain set to:\t");
  Serial.print(temp &= 0xC0, HEX);
  switch(temp)
  {
    case ADC_ATTENUATE_1X:
      Serial.println(" (X/1)");
      break;
    case ADC_ATTENUATE_2X:
      Serial.println(" (X/2)");
      break;
    case ADC_ATTENUATE_3X:
      Serial.println(" (X/3)");
      break;
    case ADC_ATTENUATE_4X:
      Serial.println(" (X/4)");
      break;
    default:
      break;
  }
}

// Changes thresholds to improve detection of fingers
void tuneEdgeSensitivity()
{
  uint8_t temp = 0x00;

  Serial.println();
  Serial.println("Setting xAxis.WideZMin...");
  ERA_ReadBytes(0x0149, &temp, 1);
  Serial.print("Current value:\t");
  Serial.println(temp, HEX);
  ERA_WriteByte(0x0149,  0x04);
  ERA_ReadBytes(0x0149, &temp, 1);
  Serial.print("New value:\t");
  Serial.println(temp, HEX);

  Serial.println();
  Serial.println("Setting yAxis.WideZMin...");
  ERA_ReadBytes(0x0168, &temp, 1);
  Serial.print("Current value:\t");
  Serial.println(temp, HEX);
  ERA_WriteByte(0x0168,  0x03);
  ERA_ReadBytes(0x0168, &temp, 1);
  Serial.print("New value:\t");
  Serial.println(temp, HEX);
}

// This function identifies when a finger is "hovering" so your system can choose to ignore them.
// Explanation: Consider the response of the sensor to be flat across it's area. The Z-sensitivity of the sensor projects this area
// a short distance upwards above the surface of the sensor. Imagine it is a solid cylinder (wider than it is tall)
// in which a finger can be detected and tracked. Adding a curved overlay will cause a user's finger to dip deeper in the middle, and higher
// on the perimeter. If the sensitivity is tuned such that the sensing area projects to the highest part of the overlay, the lowest
// point will likely have excessive sensitivity. This means the sensor can detect a finger that isn't actually contacting the overlay in the shallower area.
// ZVALUE_MAP[][] stores a lookup table in which you can define the Z-value and XY position that is considered "hovering". Experimentation/tuning is required.
// NOTE: Z-value output decreases to 0 as you move your finger away from the sensor, and it's maximum value is 0x63 (6-bits).
void Pinnacle_CheckValidTouch(absData_t * touchData)
{
  uint32_t zone_x, zone_y;
  //eliminate hovering
  zone_x = touchData->xValue / ZONESCALE;
  zone_y = touchData->yValue / ZONESCALE;
  touchData->hovering = !(touchData->zValue > ZVALUE_MAP[zone_y][zone_x]);
}

/*  ERA (Extended Register Access) Functions  */
// Reads <count> bytes from an extended register at <address> (16-bit address),
// stores values in <*data>
void ERA_ReadBytes(uint16_t address, uint8_t * data, uint16_t count)
{
  uint8_t ERAControlValue = 0xFF;

  Pinnacle_EnableFeed(false); // Disable feed

  RAP_Write(0x1C, (uint8_t)(address >> 8));     // Send upper byte of ERA address
  RAP_Write(0x1D, (uint8_t)(address & 0x00FF)); // Send lower byte of ERA address

  for(uint16_t i = 0; i < count; i++)
  {
    RAP_Write(0x1E, 0x05);  // Signal ERA-read (auto-increment) to Pinnacle

    // Wait for status register 0x1E to clear
    do
    {
      RAP_ReadBytes(0x1E, &ERAControlValue, 1);
    } while(ERAControlValue != 0x00);

    RAP_ReadBytes(0x1B, data + i, 1);

    Pinnacle_ClearFlags();
  }
}

// Writes a byte, <data>, to an extended register at <address> (16-bit address)
void ERA_WriteByte(uint16_t address, uint8_t data)
{
  uint8_t ERAControlValue = 0xFF;

  Pinnacle_EnableFeed(false); // Disable feed

  RAP_Write(0x1B, data);      // Send data byte to be written

  RAP_Write(0x1C, (uint8_t)(address >> 8));     // Upper byte of ERA address
  RAP_Write(0x1D, (uint8_t)(address & 0x00FF)); // Lower byte of ERA address

  RAP_Write(0x1E, 0x02);  // Signal an ERA-write to Pinnacle

  // Wait for status register 0x1E to clear
  do
  {
    RAP_ReadBytes(0x1E, &ERAControlValue, 1);
  } while(ERAControlValue != 0x00);

  Pinnacle_ClearFlags();
}

/*  RAP Functions */

void RAP_Init()
{
  pinMode(CS_PIN, OUTPUT);
  SPI.begin();
}

// Reads <count> Pinnacle registers starting at <address>
void RAP_ReadBytes(byte address, byte * data, byte count)
{
  byte cmdByte = READ_MASK | address;   // Form the READ command byte

  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE1));

  Assert_CS();
  SPI.transfer(cmdByte);  // Signal a RAP-read operation starting at <address>
  SPI.transfer(0xFC);     // Filler byte
  SPI.transfer(0xFC);     // Filler byte
  for(byte i = 0; i < count; i++)
  {
    data[i] =  SPI.transfer(0xFC);  // Each subsequent SPI transfer gets another register's contents
  }
  DeAssert_CS();

  SPI.endTransaction();
}

// Writes single-byte <data> to <address>
void RAP_Write(byte address, byte data)
{
  byte cmdByte = WRITE_MASK | address;  // Form the WRITE command byte

  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE1));

  Assert_CS();
  SPI.transfer(cmdByte);  // Signal a write to register at <address>
  SPI.transfer(data);    // Send <value> to be written to register
  DeAssert_CS();

  SPI.endTransaction();
}

/*  Logical Scaling Functions */
// Clips raw coordinates to "reachable" window of sensor
// NOTE: values outside this window can only appear as a result of noise
void ClipCoordinates(absData_t * coordinates)
{
  if(coordinates->xValue < PINNACLE_X_LOWER)
  {
    coordinates->xValue = PINNACLE_X_LOWER;
  }
  else if(coordinates->xValue > PINNACLE_X_UPPER)
  {
    coordinates->xValue = PINNACLE_X_UPPER;
  }
  if(coordinates->yValue < PINNACLE_Y_LOWER)
  {
    coordinates->yValue = PINNACLE_Y_LOWER;
  }
  else if(coordinates->yValue > PINNACLE_Y_UPPER)
  {
    coordinates->yValue = PINNACLE_Y_UPPER;
  }
}

// Scales data to desired X & Y resolution
void ScaleData(absData_t * coordinates, uint16_t xResolution, uint16_t yResolution)
{
  uint32_t xTemp = 0;
  uint32_t yTemp = 0;

  ClipCoordinates(coordinates);

  xTemp = coordinates->xValue;
  yTemp = coordinates->yValue;

  // translate coordinates to (0, 0) reference by subtracting edge-offset
  xTemp -= PINNACLE_X_LOWER;
  yTemp -= PINNACLE_Y_LOWER;

  // scale coordinates to (xResolution, yResolution) range
  coordinates->xValue = (uint16_t)(xTemp * xResolution / PINNACLE_X_RANGE);
  coordinates->yValue = (uint16_t)(yTemp * yResolution / PINNACLE_Y_RANGE);
}

/*  I/O Functions */
void Assert_CS()
{
  digitalWrite(CS_PIN, LOW);
}

void DeAssert_CS()
{
  digitalWrite(CS_PIN, HIGH);
}

void AssertSensorLED(bool state)
{
  digitalWrite(LED_0, !state);
}

bool DR_Asserted()
{
  return digitalRead(DR_PIN);
}

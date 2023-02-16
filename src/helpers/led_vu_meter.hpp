// #pragma once
//
// #include "../config.h"
//
// namespace UI_Utils
// {
//
//   // single-led "vumeter" glow effect
//   struct vumeter_builtin_led_t
//   {
//     int pin {-1};
//     int freq_hz {5000};
//     int channel {1};
//     int resolution {8};
//     uint8_t on_value {0x00};
//     uint8_t off_value {0xff};
//     // attach pwm to channel
//     void init()
//     {
//       ledcSetup(channel, freq_hz, resolution);
//       ledcAttachPin(pin, channel);
//       off();
//     }
//     // detach
//     void deinit()
//     {
//       ledcDetachPin(pin);
//     }
//     void setFreq( int _freq_hz )
//     {
//       _freq_hz = std::max( 10, std::min(_freq_hz, 5000) );
//       if( _freq_hz != freq_hz ) {
//         freq_hz = _freq_hz;
//         if( !ledcSetup(channel, freq_hz, resolution) ) {
//           log_e("Can't set freq_hz %d for channel %d", freq_hz, channel );
//         }
//       }
//     }
//     // positive values between 0 (dark) and 1 (bright)
//     void set( float value, int _freq_hz )
//     {
//       setFreq( _freq_hz );
//       value = std::max( value, std::min(value, 1.0f) );
//       uint8_t brightness = map( value*100, 0, 100, off_value, on_value );
//       ledcWrite(channel, brightness);
//     }
//     // turn led off
//     void off()
//     {
//       if( pin ) ledcWrite(channel, off_value);
//     }
//
//   };
//
//   extern vumeter_builtin_led_t led_meter;
//
// };

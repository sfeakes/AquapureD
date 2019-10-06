
#define GPIO_DEVICE_C_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"
#include "GPIO_Pi.h"
#include "GPIO_device.h"
#include "aq_mqtt.h"
#include "minIni.h"


struct gpiodata _gpiodata_;

void read_gpio_config(char *cfgFile) {
  char str[100];
  int i;

  // Find out how many GPIO's to configure
  for (i=1; i <= 24; i++) // 24 = Just some arbutary number (max GPIO without expansion board)
  {
    sprintf(str, "GPIO:%d", i);
    if ( ini_getl(str, "GPIO_PIN", 0, cfgFile) <= 0 )
      break;
  }

  if (i > 1) {
    _gpiodata_.num_devices = --i;
    _gpiodata_.devices = (struct gpiodevice *) calloc(_gpiodata_.num_devices, sizeof(struct gpiodevice));
    for (i=0; i < _gpiodata_.num_devices; i++ ) {
      sprintf(str, "GPIO:%d", i+1);
      ini_gets(str, "GPIO_NAME", "None", _gpiodata_.devices[i].name, sizearray(_gpiodata_.devices[i].name), cfgFile);
      _gpiodata_.devices[i].pin = ini_getl(str, "GPIO_PIN", 0, cfgFile);
      _gpiodata_.devices[i].on_state = ini_getl(str, "GPIO_PULL_UPDN", 0, cfgFile);
      _gpiodata_.devices[i].pullupdn = ini_getl(str, "GPIO_OFF_STATE", 0, cfgFile);
      _gpiodata_.devices[i].input_output = OUTPUT;
    }
  } else {
    _gpiodata_.num_devices = 0;
  }
}

bool init_gpio_device() {
  int i;

  gpioSetup();
  /*
  _gpiodata_.num_devices = 2;
  _gpiodata_.devices = (struct gpiodevice *) calloc(_gpiodata_.num_devices, sizeof(struct gpiodevice));
  //_gpiodata_.devices = calloc (1, sizeof(struct gpiodevice) + 2 * sizeof(gpiodevice*));
  //struct gpiodevice* array = malloc(2 * sizeof(struct gpiodevice));

  _gpiodata_.devices[0].pin = 13;
  _gpiodata_.devices[0].on_state = HIGH;
  _gpiodata_.devices[0].pullupdn = PUD_OFF;
  sprintf(_gpiodata_.devices[0].name, "Relay 1");
  //_gpiodata_.devices[0].changed = true;
  _gpiodata_.devices[0].input_output = OUTPUT;

  _gpiodata_.devices[1].pin = 19;
  _gpiodata_.devices[1].on_state = HIGH;
  _gpiodata_.devices[1].pullupdn = PUD_OFF;
  sprintf(_gpiodata_.devices[1].name, "Relay 2");
  //_gpiodata_.devices[1].changed = true;
  _gpiodata_.devices[1].input_output = OUTPUT;

  */
  for (i=0; i < _gpiodata_.num_devices; i++) {
    logMessage(LOG_DEBUG, "Setting GPIO %d to %s\n",_gpiodata_.devices[i].pin,_gpiodata_.devices[i].input_output==OUTPUT?"Output":"Input");
    pinMode (_gpiodata_.devices[i].pin, _gpiodata_.devices[i].input_output);
    logMessage(LOG_DEBUG, "Setting GPIO %d to Off (%d)\n",_gpiodata_.devices[i].pin,!_gpiodata_.devices[i].on_state);
    digitalWrite(_gpiodata_.devices[i].pin, !_gpiodata_.devices[i].on_state);
  }

  return true;
}

void set_gpio_uptodate() {
   _gpiodata_.changed = false;
}

bool is_gpiodevice_on(struct gpiodevice *device) {
  return (digitalRead(device->pin)==device->on_state?true:false);
}

bool set_gpio_on(struct gpiodevice *device, bool toOn) {

  if (toOn)
    digitalWrite(device->pin, device->on_state);
  else
    digitalWrite(device->pin, !device->on_state);

  logMessage(LOG_DEBUG, "Set GPIO %d '%s' to %s\n",device->pin, device->name, toOn?"On":"Off");
  //device->changed = true;
  _gpiodata_.changed = true;

  return true;
}

bool action_gpio_request(const char *command, char *value)
{
  int i;
  bool found=false;
  // Get GPIO #
  int gpio = atoi(&command[5]);

  for (i=0; i < _gpiodata_.num_devices; i++ ) {
    if (_gpiodata_.devices[i].pin == gpio) {
      if (strcmp(value, "on") == 0 ) {
        set_gpio_on(&_gpiodata_.devices[i], true);
      } else if (strcmp(value, "off") == 0 ) {
        set_gpio_on(&_gpiodata_.devices[i], false);
      } else if ( atoi(value) == MQTT_ON ) {
        set_gpio_on(&_gpiodata_.devices[i], true);
      } else if ( atoi(value) == MQTT_OFF ) {
        set_gpio_on(&_gpiodata_.devices[i], false);
      }
      found = true;
      break;
    }
  }
  if (found == false) {
    logMessage(LOG_DEBUG, "Couldn't find GPIO device for %s\n",command);
    return false;
  }
  return true;
}
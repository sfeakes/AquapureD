#ifndef GPIO_DEVICE_H_
#define GPIO_DEVICE_H_

//#include "GPIO_Pi.h"

#ifndef GPIO_DEVICE_C_
const extern struct gpiodata _gpiodata_;
#endif

struct gpiodevice
{
  char name[20];
  unsigned int pin;
  unsigned int on_state;
  unsigned int pullupdn;
  unsigned int input_output;
  //bool changed;
};

struct gpiodata
{
  int num_devices;
  struct gpiodevice *devices;
  bool changed;
};

bool init_gpio_device();
void set_gpio_uptodate();
bool is_gpiodevice_on(struct gpiodevice *device);
bool action_gpio_request(const char *command, char *value);
void read_gpio_config(char *cfgFile);

#endif //GPIO_DEVICE_H_
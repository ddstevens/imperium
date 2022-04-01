/*
Imperium - An all in one arcade USB adapter
Copyright (C) 2022  Doug Stevens

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>

#include "parson.h"

#define VENDOR  0x1234
#define PRODUCT 0x0000

typedef struct {

   uint16_t type;
   uint16_t code;
   int32_t value;
   int32_t min;
   int32_t max;

} Input;

typedef struct {

   int fd;
   char name[80];
   uint16_t vendor;
   uint16_t product;
   uint16_t version;

   uint8_t numInputs;
   Input *input;

} Device;

typedef struct {
   uint8_t numDevices;
   Device *device;
} DeviceGroup;

typedef struct {
   uint8_t changeConfigButton;
   uint8_t numDeviceGroups;
   DeviceGroup *deviceGroup;
} ControllerConfig;

typedef struct {
   uint32_t inputs[2];
   int8_t spinner[8];
   uint8_t paddle[8];
} ControllerData;

int setupDevice(Device *device);
int updateDevice(Device *device, ControllerData *cd);
int openHidRaw();
int loadConfiguration();
int loadDeviceListConfig(DeviceGroup *deviceGroup, JSON_Array *deviceJsonArray);
int loadDeviceConfig(Device *device, JSON_Object *deviceJson);
void printBits(uint32_t num);

ControllerConfig controllerConfig;
DeviceGroup alwaysActiveDevices;

bool debug = false;

int main(int argc, char *argv[])
{
   if (argc == 2 && !strcmp("debug", argv[1]))
   {
       debug = true;
   }

   // Variable to track the mode button
   bool pressed = false;
   uint8_t currentMode = 0;

   // Parse the JSON config file

   loadConfiguration();

/*
   // Run the background

   if (daemon(0, 1)) {
      perror("daemon()");
      exit(1);
   }
*/

   // Setup the user input devices

   for (int i = 0; i < controllerConfig.numDeviceGroups; i++)
   {
      for (int j = 0; j < controllerConfig.deviceGroup[i].numDevices; j++)
      {
         printf("Creating device %s\n", controllerConfig.deviceGroup[i].device[j].name);
         setupDevice(&controllerConfig.deviceGroup[i].device[j]);
      }
   }
   for (int i = 0; i < alwaysActiveDevices.numDevices; i++)
   {
      setupDevice(&alwaysActiveDevices.device[i]);
   }

   // On start try to connect to the controller regardless what udev thinks

   int fd = openHidRaw();
   /*
   if (fd < 0)
   {
      perror("Controller not found");
      return 1;
   }
   */

   // Grab data from the controller and update the user input devices

   int res;
   unsigned char buf[256];

   while(1) {
      res = read(fd, buf, sizeof(buf));
      if (res < 0)
      {
         // Probably disconnected, try to re-init
         close(fd);

         // Give some time just to let things settle
         sleep(1);

         // Only try if udev thinks it's plugged in
         while (access( "/var/run/imperium", F_OK ) < 0)
         {
             printf("Waiting for reconnect\n");
             sleep(1);
         }

         fd = openHidRaw();
         continue;
      }

      ControllerData *cd = (ControllerData *)buf;

      if (debug)
      {
         printf("Buttons = ");
         printBits(cd->inputs[1]);
         printf(" ");
         printBits(cd->inputs[0]);
         printf(" P1 = %u P2 = %u P3 = %u P4 = %u\n", cd->paddle[0], cd->paddle[1], cd->paddle[2], cd->paddle[3]);
      }

      if (cd->inputs[controllerConfig.changeConfigButton / 32] & (1 << (controllerConfig.changeConfigButton % 32))) {
         pressed = true;
      } else if (pressed) {
         currentMode++;

         if (currentMode >= controllerConfig.numDeviceGroups) currentMode = 0;

         pressed = false;

         printf("Mode %d\n", currentMode);
      }

      // Update the devices active with the current mode
      for (int i = 0; i < controllerConfig.deviceGroup[currentMode].numDevices; i++)
      {
          updateDevice(&controllerConfig.deviceGroup[currentMode].device[i], cd);
      }

      // Update the devices always active
      for (int i = 0; i < alwaysActiveDevices.numDevices; i++)
      {
          updateDevice(&alwaysActiveDevices.device[i], cd);
      }
   }

   close(fd);

   for (int i = 0; i < controllerConfig.numDeviceGroups; i++)
   {
      for (int j = 0; j < controllerConfig.deviceGroup[i].numDevices; j++)
      {
         ioctl(controllerConfig.deviceGroup[i].device[j].fd, UI_DEV_DESTROY);
         close(controllerConfig.deviceGroup[i].device[j].fd);
      }
   }

   for (int i = 0; i < alwaysActiveDevices.numDevices; i++)
   {
      ioctl(alwaysActiveDevices.device[i].fd, UI_DEV_DESTROY);
      close(alwaysActiveDevices.device[i].fd);
   }

   return 0;
}

int setupDevice(Device *device)
{
   struct uinput_setup usetup;

   memset(&usetup, 0, sizeof(usetup));
   usetup.id.bustype = BUS_USB;
   usetup.id.vendor = device->vendor;
   usetup.id.product = device->product;
   usetup.id.version = device->version;
   strcpy(usetup.name, device->name);

   device->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

   bool hasKey = false;
   bool hasRelative= false;
   bool hasAbsolute = false;

   for (int i = 0; i < device->numInputs; i++)
   {
      Input input = device->input[i];

      if (input.type == EV_KEY)
      {
         hasKey = true;
         ioctl(device->fd, UI_SET_KEYBIT, input.code);
      }
      else if (input.type == EV_REL)
      {
         hasRelative = true;
         ioctl(device->fd, UI_SET_RELBIT, input.code);
      }
      else if (input.type == EV_ABS)
      {
         hasAbsolute = true;
         struct uinput_abs_setup s =
         {
            .code = input.code,
            .absinfo = { .minimum = input.min,  .maximum = input.max },
         };

         ioctl(device->fd, UI_SET_ABSBIT, input.code);
         ioctl(device->fd, UI_ABS_SETUP, &s);
      }
   }

   if (hasKey) ioctl(device->fd, UI_SET_EVBIT, EV_KEY);
   if (hasRelative) ioctl(device->fd, UI_SET_EVBIT, EV_REL);
   if (hasAbsolute) ioctl(device->fd, UI_SET_EVBIT, EV_ABS);

   ioctl(device->fd, UI_DEV_SETUP, &usetup);
   ioctl(device->fd, UI_DEV_CREATE);

   return 0;
}

int updateDevice(Device *device, ControllerData *cd)
{
   struct input_event ev[device->numInputs + 1];
   memset(&ev, 0, sizeof ev);

   for (int i = 0; i < device->numInputs; i++)
   {
      Input input = device->input[i];

      ev[i].type = input.type;
      ev[i].code = input.code;

      if (input.type == EV_KEY) // Button, Joystick, Key
      {
         ev[i].value = (cd->inputs[input.value / 32] & (1 << (input.value % 32))) != 0;
         if (debug && ev[i].value) printf("%d\n", input.code);
      }
      else if (input.type == EV_REL) // Spinner
      {
         ev[i].value = cd->spinner[input.value];
      }
      else if (input.type == EV_ABS) // Paddle
      {
         ev[i].value = cd->paddle[input.value];
      }
   }

   ev[device->numInputs].type = EV_SYN;
   ev[device->numInputs].code = SYN_REPORT;
   ev[device->numInputs].value = 0;

   write(device->fd, &ev, sizeof ev);

   return 0;
}

// Find and open the correct hidraw device

int openHidRaw()
{
   int fd;
   DIR *d;
   struct dirent *dir;
   struct hidraw_devinfo hrdi = { 0 };
   char buf[1024];

   d = opendir("/dev");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if (strncmp("hidraw", dir->d_name, 6) == 0)
         {
            printf("Scanning %s for our device\n", dir->d_name);

            sprintf(buf, "%s%s", "/dev/", dir->d_name);

            fd = open(buf, O_RDWR);

            // Did we open it?
            if (fd < 0) continue;

            // Retrieve HID info
            if (ioctl(fd, HIDIOCGRAWINFO, &hrdi) < 0)
            {
               close(fd);
               continue;
            }

            // Did we find a match?
            if (hrdi.vendor == VENDOR && hrdi.product == PRODUCT)
            {
               printf("Found our device at %s\n", dir->d_name);
               closedir(d);
               return fd;
            }
         }
      }
      closedir(d);
   }

   return(-1);
}

// Load configuration from imperium.json

int loadConfiguration()
{
   JSON_Value *value = json_parse_file_with_comments("imperium.json");

   JSON_Object *controllerConfigJson = json_object_get_object(json_object(value), "controllerConfig");

   controllerConfig.changeConfigButton = json_object_get_number(controllerConfigJson, "changeConfigButton");

   JSON_Array *deviceGroupJsonArray = json_object_get_array(controllerConfigJson, "deviceGroup");

   int deviceGroupCount = json_array_get_count(deviceGroupJsonArray);

   controllerConfig.numDeviceGroups = deviceGroupCount;
   controllerConfig.deviceGroup = malloc(sizeof(DeviceGroup) * deviceGroupCount);

   for (int i = 0; i < deviceGroupCount; i++) {
      JSON_Object *deviceGroupJson = json_array_get_object(deviceGroupJsonArray, i);

      JSON_Array *deviceJsonArray = json_object_get_array(deviceGroupJson, "device");

      loadDeviceListConfig(&controllerConfig.deviceGroup[i], deviceJsonArray);
   }

   JSON_Array *alwaysActiveJsonArray = json_object_get_array(json_object(value), "alwaysActive");

   loadDeviceListConfig(&alwaysActiveDevices, alwaysActiveJsonArray);

   return 0;
}

int loadDeviceListConfig(DeviceGroup *deviceGroup, JSON_Array *deviceJsonArray)
{
   int deviceCount = json_array_get_count(deviceJsonArray);

   deviceGroup->numDevices = deviceCount;
   deviceGroup->device = malloc(sizeof(Device) * deviceCount);

   for (int i = 0; i < deviceCount; i++)
   {
      JSON_Object *deviceJson = json_array_get_object(deviceJsonArray, i);
      loadDeviceConfig(&deviceGroup->device[i], deviceJson);
   }

   return 0;
}

int loadDeviceConfig(Device *device, JSON_Object *deviceJson)
{
   strcpy(device->name, json_object_get_string(deviceJson, "name"));
   device->vendor = json_object_get_number(deviceJson, "vendor");
   device->product = json_object_get_number(deviceJson, "product");
   device->version = json_object_get_number(deviceJson, "version");

   JSON_Array *inputJsonArray = json_object_get_array(deviceJson, "input");

   int inputCount = json_array_get_count(inputJsonArray);

   device->numInputs = inputCount;
   device->input = malloc(sizeof(Input) * inputCount);

   for (int k = 0; k < inputCount; k++)
   {
      JSON_Object *inputJson = json_array_get_object(inputJsonArray, k);
      device->input[k].type = json_object_get_number(inputJson, "type");
      device->input[k].code = json_object_get_number(inputJson, "code");
      device->input[k].value = json_object_get_number(inputJson, "value");
      if (device->input[k].type == EV_ABS)
      {
          device->input[k].min = json_object_get_number(inputJson, "min");
          device->input[k].max = json_object_get_number(inputJson, "max");
      }
   }

   return 0;
}

void printBits(uint32_t num)
{
   for(int bit=31; bit >= 0; bit--)
   {
      printf("%i", ((num >> bit)  & 0x01));
   }
}

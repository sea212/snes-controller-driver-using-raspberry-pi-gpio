/*  Snes Controller Driver
 *  A snes controller driver which uses the gpios of the raspberry pi
 *  to poll the information which will be transfered to the input subsystem
 *  event handlers
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/input.h>	// get into the input subsystem
#include <linux/input-polldev.h>	// controller data has to be polled
#include <linux/delay.h>	// sleep function


MODULE_AUTHOR("Harald Heckmann");
MODULE_DESCRIPTION("SNES Controller driver using RPis GPIOs.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");


#define BCM2708_2835_PBASE 0xF2000000 	// Periphial Base Adress (Kernel Space)
#define GPIO_BASE (BCM2708_2835_PBASE + 0x00200000)	// absolute GPIO Baseadress
#define GPFSEL1 0x4 // GPIO Function definition Addr for Pin 10-19
#define GPFSEL2 0x8 // GPIO Function definition Addr for Pin 20-29
// note: bit 31 && 30 are reserved, else there are 3 bits for the mode
// 000 = input, 001 = output, from 010 to 111 are alternate functions (e.g. SPI)
#define GPSET0 0x1C	// set pin to high - Pins 0-31
#define GPCLR0 0x28	// clear pin to low - Pins 0-31
#define GPLEV0 0x34 // read pin - Pins 0-31

// GPIO numbers
#define GPIO_CLOCK 17
#define GPIO_LATCH 27
#define GPIO_DATA 22

static volatile uint32_t *gpio_setfunction1 = (uint32_t*) (GPIO_BASE + GPFSEL1);
static volatile uint32_t *gpio_setfunction2 = (uint32_t*) (GPIO_BASE + GPFSEL2);
static volatile uint32_t *gpio_sethigh = (uint32_t*) (GPIO_BASE + GPSET0);
static volatile uint32_t *gpio_setlow = (uint32_t*) (GPIO_BASE + GPCLR0);
static volatile uint32_t *gpio_read = (uint32_t*) (GPIO_BASE + GPLEV0);

// mapping of buttons numbers to buttons
static const int button_mapping[12] = {BTN_X, BTN_A, BTN_SELECT,\
BTN_START, ABS_HAT0Y, ABS_HAT0Y, ABS_HAT0X, ABS_HAT0X,\
BTN_Y, BTN_B, BTN_TL, BTN_TR};

// struct needed for input.h (lib that actually includes our controller into the
// input subsystem)
static struct input_dev *snes_dev;
// struct needed for input-polldev.h (our device is old and needs to be polled)
static struct input_polled_dev *snes_polled_dev;

static inline void reset_pins(void) {
	// all bits equal to zero will be ignored
	*gpio_sethigh = (1 << GPIO_CLOCK);
	*gpio_setlow = (1 << GPIO_LATCH);
}

static void init_pins(void) {
	// set function of Pin GPIO_CLOCK (GPIO 17) to output
	// clear bits: 21, 22, 23
	*gpio_setfunction1 &= ~(7 << 21);
	// set bits: 21 (001 = OUTPUT)
	*gpio_setfunction1 |= (1 << 21);
	// set function of Pin GPIO_LATCH (GPIO 27) to output
	// clear bits: 21, 22, 23
	*gpio_setfunction2 &= ~(7 << 21);
	// set bits: 21 (001 = OUTPUT)
	*gpio_setfunction2 |= (1 << 21);
	// set function of Pin GPIO_DATA (GPIO 22) to input 
	// clear bits: 3, 4, 5
	*gpio_setfunction2 &= ~(7 << 3);
	// set bits: (000 = INPUT)
	// here we would set the bits but there is nothing to set
	// reset pins now
	reset_pins();
}

static inline void uninit_pins(void) {
	// all we want to do here is to make sure the output pins are toggled off
	*gpio_setlow = (1 << GPIO_CLOCK);
	*gpio_setlow = (1 << GPIO_LATCH);
}

static void poll_snes(void) {
	int current_button = 0;
	uint16_t data = 0;

	// rise LATCH
	*gpio_sethigh = (1 << GPIO_LATCH);
	// wait for the controller to react
	usleep_range(12, 18);
	// pull latch down again
	*gpio_setlow = (1 << GPIO_LATCH);
	// wait for the controller to react
	usleep_range(6, 12);
	// begin a loop over the 16 clock cylces and poll the data

	for (current_button=0; current_button < 16; current_button++) {
		// we read (data in active low)
		if (((*gpio_read) & (1 << GPIO_DATA)) == 0) {
			printk(KERN_DEBUG "button %u has been pressed\n", current_button);
			data |= (1 << current_button);
		}

		// do one clockcycle (with delays for the controller)
		*gpio_setlow = (1 << GPIO_CLOCK);
		udelay(6);
		*gpio_sethigh = (1 << GPIO_CLOCK);
		udelay(6);
	}

	// now we're going to report the polled data (only 12 buttons, 4 undefined
	// will be ignored - they're pulled to ground anyways)
	for (current_button=0; current_button < 12; current_button++) {
		// if the button is a direction from the DPAD, then the data will get
		// special threatment
		if (current_button >= 4 && current_button <= 7) {
			switch (current_button) {
				case 4:
					if ((data & (1 << current_button)) != 0) {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], -1);
						// if we press up the button down is not required to be checked anymore
						++current_button;
					}	
					continue;
				case 5:
					if ((data & (1 << current_button)) != 0) {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], 1);
					} else {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], 0);
					}
					continue;
				case 6:
					if ((data & (1 << current_button)) != 0) {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], -1);
						// if we press left the button right is not required to be checked anymore
						++current_button;
					}	
					continue;
				case 7:
					if ((data & (1 << current_button)) != 0) {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], 1);
					} else {
						input_event(snes_dev, EV_ABS, button_mapping[current_button], 0);
					}
					continue;
			}
		}

		if ((data & (1 << current_button)) != 0) {
			input_event(snes_dev, EV_KEY, button_mapping[current_button], 1);
		} else {
			input_event(snes_dev, EV_KEY, button_mapping[current_button], 0);
		}

	}

	input_sync(snes_dev);
}

static void fill_input_dev(void) {
	snes_dev->name = "SNES-Controller";
	// define EV_KEY type events
	set_bit(EV_KEY, snes_dev->evbit);
	set_bit(EV_ABS, snes_dev->evbit);
	set_bit(BTN_B, snes_dev->keybit);
	set_bit(BTN_Y, snes_dev->keybit);
	set_bit(BTN_SELECT, snes_dev->keybit);
	set_bit(BTN_START, snes_dev->keybit);
	set_bit(BTN_X, snes_dev->keybit);
	set_bit(BTN_A, snes_dev->keybit);
	set_bit(BTN_TL, snes_dev->keybit);
	set_bit(BTN_TR, snes_dev->keybit);
	set_bit(ABS_HAT0X, snes_dev->absbit);
	set_bit(ABS_HAT0Y, snes_dev->absbit);

	input_set_abs_params(snes_dev, ABS_HAT0X, -1, 1, 0, 0);
	input_set_abs_params(snes_dev, ABS_HAT0Y, -1, 1, 0, 0);
	
	// I could not find the product id
	snes_dev->id.bustype = BUS_HOST;
	snes_dev->id.vendor = 0x12E1;
	snes_dev->id.product = 0x0001;
	snes_dev->id.version = 0x001;
}


static int __init init_snes(void) {
	int error;

	printk(KERN_INFO "[SNES] initialising\n");
	printk(KERN_DEBUG "[SNES] Initialising GPIO-Pins...\n");
	init_pins();

	printk(KERN_DEBUG "[SNES] Allocating structure input_dev...\n");
	snes_dev = input_allocate_device();
	if (!snes_dev) {
		printk(KERN_ERR "snes.c: Not enough memory\n");
		return -ENOMEM;
	}

	printk(KERN_DEBUG "[SNES] Filling structure input_dev...\n");
	fill_input_dev();

	printk(KERN_DEBUG "[SNES] Allocating structure snes_polled_dev...\n");
	snes_polled_dev = input_allocate_polled_device();
	if (!snes_polled_dev) {
		printk(KERN_ERR "snes.c: Not enough memory\n");
		return -ENOMEM;
	}

	// when you are using input.h, you should define open() and close()
	// functions when your devices needs to be polled, so the polling
	// begins when the virtual joystick file is being read
	// luckily it is already implemented in input-polldev

	printk(KERN_DEBUG "[SNES] Filling structure snes_polled_dev...\n");
	snes_polled_dev->poll = (void*)poll_snes;
	snes_polled_dev->poll_interval = 16; // msec ~ 60 HZ
	snes_polled_dev->poll_interval_max = 32;
	snes_polled_dev->input = snes_dev;

	printk(KERN_DEBUG "[SNES] Registering polled device...\n");
	error = input_register_polled_device(snes_polled_dev);
	if (error) {
		printk(KERN_ERR "snes.c: Failed to register polled device\n");
		input_free_polled_device(snes_polled_dev);
		return error;
	}

	printk(KERN_INFO "[SNES] initialised\n");
	return 0;
}


static void __exit uninit_snes(void) {
	printk(KERN_INFO "[SNES] uninitialising\n");
	uninit_pins();
	input_unregister_polled_device(snes_polled_dev);
	input_free_polled_device(snes_polled_dev);
	input_free_device(snes_dev);
	printk(KERN_INFO "[SNES] Good Bye Kernel :'(\n");
}


module_init(init_snes);
module_exit(uninit_snes);

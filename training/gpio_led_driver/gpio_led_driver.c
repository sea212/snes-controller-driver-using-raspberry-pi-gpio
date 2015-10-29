/*  gpio_led_driver - low level gpio controlling
 *  write high on one pin, using BCM 2835 ARM Periphial Reference (it's a test)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h> 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Heckmann");
MODULE_DESCRIPTION("A Linux Driver to configure one pin as output and write on it.");
MODULE_VERSION("0.1");


#define BCM2708_2835_PBASE 0xF2000000 	// Periphial Base Adress (Kernel Space)
#define GPIO_BASE (BCM2708_2835_PBASE + 0x00200000)	// absolute GPIO Baseadress
#define GPFSEL0 0x0 					// GPIO Function definition Addr for Pin 0-9
// note: bit 31 && 30 are reserved, else there are 3 bits for the mode
// 000 = input, 001 = output, from 010 to 111 are alternate functions (e.g. SPI)
#define GPSET0 0x1C	// set pin to high - Pins 
#define GPCLR0 0x28	// clear pin to low - Pins 
#define GPLEV0 0x34 // read pin

static volatile uint32_t *gpio_setfunction = (uint32_t*) (GPIO_BASE + GPFSEL0);
static volatile uint32_t *gpio_sethigh = (uint32_t*) (GPIO_BASE + GPSET0);
static volatile uint32_t *gpio_setlow = (uint32_t*) (GPIO_BASE + GPCLR0);
static volatile uint32_t *gpio_read = (uint32_t*) (GPIO_BASE + GPLEV0);

static int init_pins(void)
{
	printk(KERN_INFO "Initialising GPIO-Pins\nStep1: Configure Functions\n");
	// configure pin functions for pins 1-9 (actually we set 8 to output and leave the rest)
	// clear bits: 24, 25, 26
	*gpio_setfunction &= ~(7 << 24);
	// set bits: 24
	*gpio_setfunction |= (1 << 24);
	printk(KERN_INFO "Step2: Set GPIO-Pin 8 to high\n");
	// set pin 8 to high
	*gpio_sethigh = (1 << 8);
	// active_low logic?
	printk(KERN_INFO "Step3: Evaluate. GPIO-Pin 8 is on: %s\n", (((*gpio_read) & (1 << 8)) ? "low" : "high"));
	return 0;
}


static void uninit_pins(void)
{
	printk(KERN_INFO "setting voltage on used pins to low and leaving\n");
	// before we leave the kernel, set the outputpin (8) to low
	*gpio_setlow = (1 << 8);
	printk(KERN_INFO "GPIO-Pin 8 is on: %s\n", (((*gpio_read) & (1 << 8)) ? "low" : "high"));
}


module_init(init_pins);
module_exit(uninit_pins);

MODULE_LICENSE("GPL");

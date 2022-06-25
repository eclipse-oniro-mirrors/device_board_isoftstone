#ifndef __SMDTIO_H__
#define __SMDTIO_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/fs.h>


/*IOCTL set*/
#define IOCTL_USBOTG_SET_POWER               0xA3
#define IOCTL_USBHOST1_SET_POWER             0xA4
#define IOCTL_USBHOST2_SET_POWER             0xA5
#define IOCTL_USBHOST3_SET_POWER             0xA6
#define IOCTL_USBHOST4_SET_POWER             0xA7
#define IOCTL_GPIO_SET_DIRECTION             0xE3
#define IOCTL_GPIO_SET_VALUE                 0xE4

/*IOCTL get*/
#define IOCTL_USBOTG_GET_POWER              (IOCTL_USBOTG_SET_POWER      -0x80)
#define IOCTL_USBHOST1_GET_POWER      		(IOCTL_USBHOST1_SET_POWER    -0x80)
#define IOCTL_USBHOST2_GET_POWER            (IOCTL_USBHOST2_SET_POWER    -0x80)
#define IOCTL_USBHOST3_GET_POWER            (IOCTL_USBHOST3_SET_POWER    -0x80)
#define IOCTL_USBHOST4_GET_POWER            (IOCTL_USBHOST4_SET_POWER    -0x80)
#define IOCTL_GPIO_GET_DIRECTION            (IOCTL_GPIO_SET_DIRECTION    -0x80)
#define IOCTL_GPIO_GET_VALUE                (IOCTL_GPIO_SET_VALUE        -0x80)


struct  smdtio_user
{
    char value;
    char direction;
    char label[16];
};

struct smdtio_gpio_desc
{
    char 		    *label;
	int 			flags;
    int 			value;
    int 			valid;
    int 			last_value;
    struct 			gpio_desc *gpio;
};

struct smdtio_priv
{
    struct device              *dev;
    struct miscdevice          miscdev;
    struct smdtio_gpio_desc    gpios[64];
	int                        gpio_count;
};

static int smdtio_set_gpio_value(struct smdtio_priv *smdtio,const char *gpio_name,int gpio_value);
static int smdtio_get_gpio_value(struct smdtio_priv *smdtio,const char *gpio_name);
static int smdtio_get_gpio_direction(struct smdtio_priv *smdtio,char *gpio_name);
static int smdtio_set_gpio_direction(struct smdtio_priv *smdtio,char *gpio_name,int direction);


/*****************
*   external     *
*****************/
#define LED_BREATH_RATE             2
#define BLUE_LED_NAME               "blue-led"
#define RED_LED_NAME                "red-led"
#define BREATH_LED_NAME             "breath-led"
#define RS485_CTRL_NAME             "rs485-ctrl"

static struct gpio_desc      *blue_led_gpio;
static struct gpio_desc      *red_led_gpio;
static struct gpio_desc      *breath_led_gpio;
static struct gpio_desc      *rs485_ctrl_gpio;

#endif

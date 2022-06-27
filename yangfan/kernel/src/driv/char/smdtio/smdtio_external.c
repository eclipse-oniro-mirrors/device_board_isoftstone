
int smdtio_set_rs485_ctrl(int value)
{
    if(rs485_ctrl_gpio)
    {
        if(value)
            gpiod_set_value(rs485_ctrl_gpio,1);
        else
            gpiod_set_value(rs485_ctrl_gpio,0);
    }
	return 0;
}
EXPORT_SYMBOL(smdtio_set_rs485_ctrl);

int smdtio_set_blink_led(int value)
{
    if(breath_led_gpio)
    {
        if(value)
            gpiod_set_value(breath_led_gpio, 1);
        else
            gpiod_set_value(breath_led_gpio, 0);
    }
    return 0;
}
EXPORT_SYMBOL(smdtio_set_blink_led);


int smdtio_get_blink_led(void)
{
    if(breath_led_gpio)
    {
        return gpiod_get_value(breath_led_gpio);
    }
    return 0;
}
EXPORT_SYMBOL(smdtio_get_blink_led);


int smdtio_set_blue_led(int value)
{
    if(blue_led_gpio)
    {
        if(value)
            gpiod_set_value(blue_led_gpio, 1);
        else
            gpiod_set_value(blue_led_gpio, 0);
    }
    return 0;
}
EXPORT_SYMBOL(smdtio_set_blue_led);


int smdtio_get_blue_led(void)
{
    if(blue_led_gpio)
    {
        return gpiod_get_value(blue_led_gpio);
    }
    return 0;
}
EXPORT_SYMBOL(smdtio_get_blue_led);

int smdtio_set_red_led(int value)
{
    if(red_led_gpio)
    {
        if(value)
            gpiod_set_value(red_led_gpio, 1);
        else
            gpiod_set_value(red_led_gpio, 0);
    }
    return 0;
}
EXPORT_SYMBOL(smdtio_set_red_led);


int smdtio_get_red_led(void)
{
    if(red_led_gpio)
        return gpiod_get_value(red_led_gpio);
    return 0;
}
EXPORT_SYMBOL(smdtio_get_red_led);


static void flip_led_wq(struct work_struct *work)
{
    int i;
    for(i=0;i<LED_BREATH_RATE*2;i++)
    {
        if(smdtio_get_blue_led()==0)
           smdtio_set_blue_led(1);
        else
            smdtio_set_blue_led(0);
        msleep(50/LED_BREATH_RATE);
    }
}
static DECLARE_DELAYED_WORK(flip_led_wq_work, flip_led_wq);

int smdtio_blink_led(void)
{
    schedule_delayed_work(&flip_led_wq_work, 0);
	return 0;
}
EXPORT_SYMBOL(smdtio_blink_led);

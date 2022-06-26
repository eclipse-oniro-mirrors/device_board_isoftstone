
int smdtio_set_usb_power(struct smdtio_priv *smdtio,int port,int value)
{
        switch(port)
        {
        case 0:
                smdtio_set_gpio_value(smdtio,"usb_drv1",value);
                break;
        case 1:
                smdtio_set_gpio_value(smdtio,"usb_drv2",value);
                break;
        case 2:
                smdtio_set_gpio_value(smdtio,"usb_drv3",value);
                break;
        case 3:
                smdtio_set_gpio_value(smdtio,"usb_drv4",value);
                break;
        case 4:
                smdtio_set_gpio_value(smdtio,"usb_drv5",value);
                break;
        default:
				printk("###[smdt] No enough USB settings !\n");
                break;
        }
        return 0;
}

int smdtio_get_usb_power(struct smdtio_priv *smdtio,int port,char *value)
{
        switch(port)
        {
        case 0:
                *value=smdtio_get_gpio_value(smdtio,"usb_drv1");
                break;
        case 1:
                *value=smdtio_get_gpio_value(smdtio,"usb_drv2");
                break;
        case 2:
                *value=smdtio_get_gpio_value(smdtio,"usb_drv3");
                break;
        case 3:
                *value=smdtio_get_gpio_value(smdtio,"usb_drv4");
                break;
        case 4:
                *value=smdtio_get_gpio_value(smdtio,"usb_drv5");
                break;
        default:
				printk("###[smdt] No enough USB to get !\n");
                break;
        }
        return 0;
}

int smdtio_set_io_power(struct smdtio_priv *smdtio,int io,int value)
{
    char buffer[64]={0};
    sprintf(buffer,"io%d",io);
    smdtio_set_gpio_value(smdtio,buffer,value);
    return 0;

}


int smdtio_get_io_power(struct smdtio_priv *smdtio,int io,int *value)
{
    char buffer[64]={0};
    sprintf(buffer,"io%d",io);
    *value=smdtio_get_gpio_value(smdtio,buffer);
    return *value;
}

static long smdtio_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	   int ret;
       struct smdtio_user smdtio_user={0};
	   struct smdtio_priv *smdtio = container_of(file->private_data, struct smdtio_priv, miscdev);
	   ret = copy_from_user((char *)&smdtio_user, (char *)arg,sizeof(struct smdtio_user));

        switch(cmd)
        {
			case IOCTL_USBOTG_SET_POWER:
					smdtio_set_usb_power(smdtio,0,smdtio_user.value);
					break;
			case IOCTL_USBOTG_GET_POWER:
					smdtio_get_usb_power(smdtio,0,&smdtio_user.value);
					break;
			case IOCTL_USBHOST1_SET_POWER:
					smdtio_set_usb_power(smdtio,1,smdtio_user.value);
					break;
			case IOCTL_USBHOST1_GET_POWER:
					smdtio_get_usb_power(smdtio,1,&smdtio_user.value);
					break;
			case IOCTL_USBHOST2_SET_POWER:
					smdtio_set_usb_power(smdtio,2,smdtio_user.value);
					break;
			case IOCTL_USBHOST2_GET_POWER:
					smdtio_get_usb_power(smdtio,2,&smdtio_user.value);
					break;
			case IOCTL_USBHOST3_SET_POWER:
					smdtio_set_usb_power(smdtio,3,smdtio_user.value);
					break;
			case IOCTL_USBHOST3_GET_POWER:
					smdtio_get_usb_power(smdtio,3,&smdtio_user.value);
					break;
			case IOCTL_USBHOST4_SET_POWER:
					smdtio_set_usb_power(smdtio,4,smdtio_user.value);
					break;
			case IOCTL_USBHOST4_GET_POWER:
					smdtio_get_usb_power(smdtio,4,&smdtio_user.value);
					break;

			case IOCTL_GPIO_SET_DIRECTION:
					smdtio_set_gpio_direction(smdtio,smdtio_user.label,smdtio_user.direction);
					smdtio_set_gpio_value(smdtio,smdtio_user.label,smdtio_user.value);
					break;
			case IOCTL_GPIO_GET_DIRECTION:
					smdtio_user.direction=smdtio_get_gpio_direction(smdtio,smdtio_user.label);
					break;
			case IOCTL_GPIO_SET_VALUE:
					smdtio_set_gpio_value(smdtio,smdtio_user.label,smdtio_user.value);
					break;
			case IOCTL_GPIO_GET_VALUE:
					smdtio_user.value=smdtio_get_gpio_value(smdtio,smdtio_user.label);
					break;
			default :
					printk("###[smdt] not support ioctl : %x\n", cmd);
					return -1;
        }

        ret= copy_to_user((char*)arg,(char *)&smdtio_user,sizeof(struct smdtio_user));
        return 0;
}

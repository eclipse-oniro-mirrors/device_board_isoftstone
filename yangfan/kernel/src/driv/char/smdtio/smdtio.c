/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */
#include "smdtio.h"
#include "smdtio_stub.c"
#include "smdtio_external.c"

static int smdtio_set_gpio_direction(struct smdtio_priv *smdtio,char *gpio_name,int direction)
{
	int i,gpio_found=0;
    for(i=0;i<smdtio->gpio_count;i++)
    {
       if(!strcmp(gpio_name,smdtio->gpios[i].label))
       {
		  gpio_found =1;
          if(direction)
              gpiod_direction_output(smdtio->gpios[i].gpio, smdtio->gpios[i].last_value);
          else
              gpiod_direction_input(smdtio->gpios[i].gpio);
          break;
       }
    }
	if(!gpio_found)
	{
		printk("###[smdt] not support set direction gpio:%s\n",gpio_name);
		return -1;
	}
    return 0;
}

static int smdtio_get_gpio_direction(struct smdtio_priv *smdtio,char *gpio_name)
{
	int i,gpio_found=0;
    for(i=0;i<smdtio->gpio_count;i++)
    {
       if(!strcmp(gpio_name,smdtio->gpios[i].label))
	   {
		   gpio_found=1;
           break;
	   }
    }
	if(!gpio_found)
	{
		 printk("###[smdt] not support set direction gpio:%s\n",gpio_name);
		 return -1;
	}
	 return !!!gpiod_get_direction(smdtio->gpios[i].gpio);
}


static int smdtio_set_gpio_value(struct smdtio_priv *smdtio,const char *gpio_name,int gpio_value)
{
    int i,gpio_found=0;
    for(i=0;i<smdtio->gpio_count;i++)
    {
        if(!strcmp(gpio_name,smdtio->gpios[i].label))
        {
            gpiod_set_value(smdtio->gpios[i].gpio, gpio_value);
            smdtio->gpios[i].last_value=gpio_value;
            gpio_found=1;
            break;
        }
    }
    if(!gpio_found)
	{
        printk("###[smdt] not support set value gpio:%s\n",gpio_name);
		return -1;
	}
	return 0;
}


static int smdtio_get_gpio_value(struct smdtio_priv *smdtio,const char *gpio_name)
{
    int i,gpio_found=0;
    for(i=0;i<smdtio->gpio_count;i++)
    {
        if(!strcmp(gpio_name,smdtio->gpios[i].label))
        {
            gpio_found=1;
            break;
        }
    }
    if(!gpio_found)
	{
       printk("###[smdt]not support get value gpio:%s\n",gpio_name);
	   return -1;
	}
	 return gpiod_get_value(smdtio->gpios[i].gpio);
}


static int smdtio_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int smdtio_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t smdtio_write(struct file *file, const char __user *data,
                                   size_t len, loff_t *ppos)
{
    return 0;
}

static ssize_t smdtio_read(struct file *file, char __user *buf, size_t len,
                                  loff_t *ppos)
{
    return len;
}

static int smdtio_parse_dt(struct smdtio_priv *smdtio,struct device_node *node)
{
        int i;
        char *gpio_names[64]={0};
        char *gpio_input_output[64]={0};
        char *gpio_default_value[64]={0};
        int gpio_names_sz=0;
        int gpio_input_output_sz=0;
        int gpio_default_value_sz=0;

		gpio_names_sz=of_property_count_strings(node, "gpio_labels");
        gpio_input_output_sz=of_property_count_strings(node, "gpio_direction");
        gpio_default_value_sz=of_property_count_strings(node, "gpio_default_value");

        of_property_read_string_array(node,"gpio_labels", (const char**)&gpio_names,gpio_names_sz);
        of_property_read_string_array(node,"gpio_direction", (const char**)&gpio_input_output,gpio_input_output_sz);
        of_property_read_string_array(node,"gpio_default_value", (const char**)&gpio_default_value,gpio_default_value_sz);

		smdtio->gpio_count=gpio_names_sz;
		if(!((gpio_names_sz==gpio_input_output_sz)&&(gpio_input_output_sz==gpio_default_value_sz)))
            return -1;
        else
        {
            for(i=0;i<gpio_names_sz;i++)
            {
                if(!strcmp("input",gpio_input_output[i]))
					smdtio->gpios[i].flags      = GPIOD_IN;
                else
                {
                    if(!strcmp("high",gpio_default_value[i]))
                        smdtio->gpios[i].flags    = GPIOD_OUT_HIGH;
					else
                        smdtio->gpios[i].flags    = GPIOD_OUT_LOW;
                }
                smdtio->gpios[i].label=gpio_names[i];
                smdtio->gpios[i].gpio=devm_gpiod_get(smdtio->dev,gpio_names[i],smdtio->gpios[i].flags);
                if(IS_ERR(smdtio->gpios[i].gpio))
                {
                    smdtio->gpios[i].valid=0;
                    dev_err(smdtio->dev,"###[smdt]request gpio:%s fail!\n",gpio_names[i]);
                }
                else
                    smdtio->gpios[i].valid=1;
            }
        }
        return 0;
}

static ssize_t data_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    struct smdtio_priv *smdtio=dev_get_drvdata(dev);
    return sprintf(buf,"%d\n",smdtio_get_gpio_value(smdtio,dev_name(dev)));
}

static ssize_t data_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
    struct smdtio_priv *smdtio=dev_get_drvdata(dev);
    unsigned long value;
    if (kstrtoul(buf, 10, &value))
               return -EINVAL;
    if(value)
            smdtio_set_gpio_value(smdtio,dev_name(dev),1);
    else
            smdtio_set_gpio_value(smdtio,dev_name(dev),0);
    return count;
}
static DEVICE_ATTR_RW(data);

static struct attribute *smdtio_attrs[] = {
        &dev_attr_data.attr,
        NULL,
};
ATTRIBUTE_GROUPS(smdtio);

static int smdtio_create_sysfs(struct smdtio_priv *smdtio)
{
        struct class *class;
        struct device *dev;
        int i;
        class = class_create(THIS_MODULE, "gpio_sw");
        if (IS_ERR(class))
            return PTR_ERR(class);

        for(i=0;i<smdtio->gpio_count;i++)
        {
            if(!smdtio->gpios[i].valid)
                continue;
            dev=device_create_with_groups(class,smdtio->dev, MKDEV(0, 0),(void *)smdtio,smdtio_groups, "%s",smdtio->gpios[i].label);
            if (IS_ERR(dev))
                return PTR_ERR(dev);
        }
        return 0;
}

static struct file_operations smdtio_fops = {

    .owner           = THIS_MODULE,
    .open            = smdtio_open,
    .release         = smdtio_close,
    .write           = smdtio_write,
    .read            = smdtio_read,
    .unlocked_ioctl  = smdtio_ioctl,
    .compat_ioctl    = smdtio_ioctl,
};

static struct miscdevice smdtio_miscdev = {

    .minor   = MISC_DYNAMIC_MINOR,
    .name    = "smdtio",
    .fops    = &smdtio_fops,

};

static int smdtio_probe(struct platform_device *pdev)
{
        struct smdtio_priv *smdtio;
        struct device_node *node = pdev->dev.of_node;
        int ret=0;

		smdtio=devm_kzalloc(&pdev->dev, sizeof(struct smdtio_priv),GFP_KERNEL);
        if (!smdtio)
            return -ENOMEM;

        smdtio->dev = &pdev->dev;
		dev_set_drvdata(&pdev->dev, smdtio);

        ret=smdtio_parse_dt(smdtio,node);
        if(ret)
            goto err;

        ret=smdtio_create_sysfs(smdtio);
        if(ret)
            goto err;

		smdtio_miscdev.parent  = smdtio->dev;
        ret=misc_register(&smdtio_miscdev);
        if(ret)
                goto err;

        return 0;
err:
        dev_err(smdtio->dev,"###[smdt]smdtio driver probe fail:%d\n",ret);
        return ret;
}


static int smdtio_remove(struct platform_device *pdev)
{
        struct smdtio_priv *smdtio = dev_get_drvdata(&pdev->dev);
        misc_deregister(&smdtio->miscdev);
        return 0;
}

static const struct of_device_id smdtio_match[] = {
        { .compatible = "smdt,smdtio", },
        {},
};

static struct platform_driver smdtio_driver = {
        .probe  = smdtio_probe,
        .remove = smdtio_remove,
        .driver = {
                .name = "smdtio",
                .of_match_table = of_match_ptr(smdtio_match),
        },
};
module_platform_driver(smdtio_driver);
MODULE_DESCRIPTION("smdtio Driver");
MODULE_LICENSE("GPL");

/*
 * Test Module Please Ignore
 *
 * Playing around with GPIO interrupts, delayed work, and more.
 *
 * Copyright 2019 Allen Wild
 * SPDX-License-Identifier: GPLv2
 */

#define pr_fmt(fmt) "WKTEST: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define MSG(fmt, args...) pr_info("%s: " fmt "\n", __func__, ##args)
#define ERR(fmt, args...) pr_err("%s: " fmt "\n", __func__, ##args)

#define GPIO_NUM 2

struct wktest_data {
    int                 count;
    int                 measure_length;
    unsigned int        irq;
    bool                irq_enabled;
    unsigned long       measure_jstart;
    unsigned long       measure_jend;
    spinlock_t          countlock;
    spinlock_t          irqlock;
    struct gpio_desc    *gpiod;
    struct delayed_work measure_work;
};

static __always_inline bool str_startswith(const char *str, const char *word)
{
    return !strncmp(str, word, strlen(word));
}

// enable_irq and disable_irq stack, but we don't want that, so we keep track
// of whether we've enabled IRQs for this
static inline void wktest_enable_irq_maybe(struct wktest_data *data)
{
    unsigned long flags;
    bool was_enabled;

    spin_lock_irqsave(&data->irqlock, flags);
    was_enabled = data->irq_enabled;
    if (!was_enabled) {
        enable_irq(data->irq);
        data->irq_enabled = true;
    }
    spin_unlock_irqrestore(&data->irqlock, flags);

    if (!was_enabled)
        MSG("Enabled IRQ");
}

static inline void wktest_disable_irq_maybe(struct wktest_data *data)
{
    unsigned long flags;
    bool was_enabled;

    spin_lock_irqsave(&data->irqlock, flags);
    was_enabled = data->irq_enabled;
    if (was_enabled) {
        disable_irq(data->irq);
        data->irq_enabled = false;
    }
    spin_unlock_irqrestore(&data->irqlock, flags);

    if (was_enabled)
        MSG("Disabled IRQ");
}

static irqreturn_t wktest_irq_handler(int irq, void *idata)
{
    struct wktest_data *data = idata;
    if (data->irq_enabled) {
        unsigned long flags;
        spin_lock_irqsave(&data->countlock, flags);
        data->count++;
        spin_unlock_irqrestore(&data->countlock, flags);
        //MSG("tick"); // unsafe in an IRQ???
    }
    return IRQ_HANDLED;
}

static void wktest_measure_end_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wktest_data *data = container_of(dwork, struct wktest_data, measure_work);
    wktest_disable_irq_maybe(data);
    data->measure_jend = jiffies;
    MSG("finished");
}

static inline void wktest_start_measurement(struct wktest_data *data)
{
    unsigned long flags;

    // if a measurement is pending, cancel it.
    // if the measurement work is already running, wait for it to finish
    cancel_delayed_work_sync(&data->measure_work);

    spin_lock_irqsave(&data->countlock, flags);
    data->count = 0;
    spin_unlock_irqrestore(&data->countlock, flags);

    data->measure_jend = 0;
    data->measure_jstart = jiffies;
    wktest_enable_irq_maybe(data);
    schedule_delayed_work(&data->measure_work, msecs_to_jiffies(data->measure_length));
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct wktest_data *data = dev_get_drvdata(dev);
    const char *state = NULL;
    long measure_time = 0;

    if (data->irq_enabled) {
        state = "counting";
    } else {
        state = "ready";
    }

    if (!state)
        return -EINVAL;

    if (data->measure_jstart && data->measure_jend) {
        measure_time = jiffies_to_msecs(data->measure_jend - data->measure_jstart);
    }
    return scnprintf(buf, 4096, "%s, gpio = %d, length = %d, mtime = %ld, count = %d\n",
                     state, gpiod_get_value(data->gpiod), data->measure_length, measure_time, data->count);
}
DEVICE_ATTR_RO(status);

static ssize_t count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct wktest_data *data = dev_get_drvdata(dev);
    return scnprintf(buf, 4096, "%d\n", data->count);
}
DEVICE_ATTR_RO(count);

static ssize_t control_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct wktest_data *data = dev_get_drvdata(dev);
    unsigned long flags;
    const char *val_str;
    int new_count = -1;

    if (sysfs_streq(buf, "reset")) {
        new_count = 0;
    } else if (sysfs_streq(buf, "start")) {
        wktest_enable_irq_maybe(data);
    } else if (sysfs_streq(buf, "stop")) {
        wktest_disable_irq_maybe(data);
    } else if (sysfs_streq(buf, "measure")) {
        wktest_start_measurement(data);
    } else if ((val_str = strchr(buf, '=')) != NULL) {
        int val;
        val_str++;
        if (kstrtoint(val_str, 0, &val) < 0) {
            return -EINVAL;
        }

        if (str_startswith(buf, "length")) {
            data->measure_length = val;
        } else {
            return -EINVAL;
        }
    }

    if (new_count != -1) {
        spin_lock_irqsave(&data->countlock, flags);
        data->count = new_count;
        spin_unlock_irqrestore(&data->countlock, flags);
    }

    return count;
}
DEVICE_ATTR_WO(control);

static struct device_attribute *wktest_attrs[] = {
    &dev_attr_status,
    &dev_attr_control,
    &dev_attr_count,
};

static int __init wktest_probe(struct platform_device *pdev)
{
    int i, err;
    struct wktest_data *data;

    MSG("enter");
    data = devm_kzalloc(&pdev->dev, sizeof(struct wktest_data), GFP_KERNEL);
    if (unlikely(!data)) {
        return -ENOMEM;
    }

    data->count = 0;
    data->measure_length = 5000;
    spin_lock_init(&data->countlock);
    spin_lock_init(&data->irqlock);
    INIT_DELAYED_WORK(&data->measure_work, wktest_measure_end_work);

    err = gpio_request(GPIO_NUM, "wktest");
    if (err) {
        ERR("gpio_get failed");
        goto err_mem;
    }
    data->gpiod = gpio_to_desc(GPIO_NUM);
    if (IS_ERR(data->gpiod)) {
        ERR("failed to get gpio desc for gpio %d", GPIO_NUM);
        err = PTR_ERR(data->gpiod);
        goto err_gpio;
    }

    dev_set_drvdata(&pdev->dev, data);
    for (i = 0; i < ARRAY_SIZE(wktest_attrs); i++) {
        err = device_create_file(&pdev->dev, wktest_attrs[i]);
        if (err) {
            ERR("Failed to create sysfs file %d", i);
            goto err_sysfs;
        }
    }

    err = gpiod_to_irq(data->gpiod);
    if (err < 0) {
        ERR("failed to get irq from gpiod");
        goto err_sysfs;
    }

    data->irq = err;
    data->irq_enabled = false; // handler will do nothing if our SW enabled flag is false
    err = request_irq(data->irq, wktest_irq_handler, IRQF_TRIGGER_FALLING, pdev->name, data);
    if (err) {
        ERR("failed to request irq");
        goto err_sysfs;
    }
    // immediately disable IRQ. If it got called before here it didn't do anything
    disable_irq(data->irq);

    MSG("complete");
    return 0;

//err_free_irq:
    //free_irq(data->irq, data);
err_sysfs:
    for (i--; i >= 0; i--) {
        device_remove_file(&pdev->dev, wktest_attrs[i]);
    }
err_gpio:
    gpio_free(GPIO_NUM);
err_mem:
    devm_kfree(&pdev->dev, data);
    ERR("return %d", err);
    return err;
}

static int __exit wktest_remove(struct platform_device *pdev)
{
    int i;
    struct wktest_data *data = dev_get_drvdata(&pdev->dev);

    free_irq(data->irq, data);
    for (i = 0; i < ARRAY_SIZE(wktest_attrs); i++) {
        device_remove_file(&pdev->dev, wktest_attrs[i]);
    }
    gpio_free(GPIO_NUM);
    devm_kfree(&pdev->dev, data);
    MSG("complete");
    return 0;
}

static struct platform_device *wktest_pdev = NULL;
static struct platform_driver wktest_driver = {
    .driver = {
        .name = "wktest", // must match platform_device's name
    },
    .remove = wktest_remove,
};

static int __init wktest_init(void)
{
    int err;
    wktest_pdev = platform_device_register_simple("wktest", -1, NULL, 0);
    if (IS_ERR(wktest_pdev)) {
        ERR("platform_device_register_simple returned %ld", PTR_ERR(wktest_pdev));
        return PTR_ERR(wktest_pdev);
    }

    err = platform_driver_probe(&wktest_driver, wktest_probe);
    if (err) {
        ERR("platform_driver_probe returned %d", err);
        platform_device_unregister(wktest_pdev);
        return err;
    }

    MSG("complete");
    return 0;
}

static void __exit wktest_exit(void)
{
    if (wktest_pdev) {
        platform_device_unregister(wktest_pdev);
        platform_driver_unregister(&wktest_driver);
    }
    MSG("complete");
}

module_init(wktest_init);
module_exit(wktest_exit);

MODULE_AUTHOR("Allen Wild <allenwild93@gmail.com>");
MODULE_DESCRIPTION("Test driver please ignore");
MODULE_LICENSE("GPL");

/*
 *  KTS1620x  I2C Port Expander with 8/16 I/O
 *
 *  Copyright (C) 2023 Andrey Mitrofanov <avmwwww@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define KTS1620X_CONFIG_PORT0		0x0c
#define KTS1620X_CONFIG_PORT1		0x0d
#define KTS1620X_CONFIG_PORT2		0x0e

#define KTS1620X_IN_PORT0		0x00
#define KTS1620X_IN_PORT1		0x01
#define KTS1620X_IN_PORT2		0x02

#define KTS1620X_OUT_PORT0		0x04
#define KTS1620X_OUT_PORT1		0x05
#define KTS1620X_OUT_PORT2		0x06

#define KTS1620X_OUT_PORT0A		0x40
#define KTS1620X_OUT_PORT0B		0x41
#define KTS1620X_OUT_PORT1A		0x42
#define KTS1620X_OUT_PORT1B		0x43
#define KTS1620X_OUT_PORT2A		0x44
#define KTS1620X_OUT_PORT2B		0x45

#define KTS1620X_PINS			8
#define KTS1620X_PORT_MAX		2
#define KTS1620X_PORTS			(KTS1620X_PORT_MAX + 1)

struct kts1620_chip {
	struct regmap *regmap;
	struct gpio_chip gpio_chip;
};

static int kts1620_gpio_direction_input(struct gpio_chip *gc, unsigned off)
{
	struct kts1620_chip *chip = gpiochip_get_data(gc);
	unsigned port = off / KTS1620X_PINS;
	unsigned pin = off % KTS1620X_PINS;

	return regmap_update_bits(chip->regmap, KTS1620X_CONFIG_PORT0 + port,
				  1u << pin, 1u << pin);
}

static int kts1620_gpio_direction_output(struct gpio_chip *gc,
		unsigned off, int val)
{
	struct kts1620_chip *chip = gpiochip_get_data(gc);
	unsigned port = off / KTS1620X_PINS;
	unsigned pin = off % KTS1620X_PINS;
	int ret;

	ret = regmap_update_bits(chip->regmap, KTS1620X_CONFIG_PORT0 + port,
				  1u << pin, 0);
	if (ret < 0)
		return ret;

	return regmap_update_bits(chip->regmap, KTS1620X_OUT_PORT0 + port,
			1u << pin, val ? (1u << pin) : 0);
}

static int kts1620_gpio_get_value(struct gpio_chip *gc, unsigned off)
{
	struct kts1620_chip *chip = gpiochip_get_data(gc);
	unsigned port = off / KTS1620X_PINS;
	unsigned pin = off % KTS1620X_PINS;
	unsigned int reg;

	regmap_read(chip->regmap, KTS1620X_IN_PORT0 + port, &reg);

	return !!(reg & (1u << pin));
}

static void kts1620_gpio_set_value(struct gpio_chip *gc, unsigned off, int val)
{
	struct kts1620_chip *chip = gpiochip_get_data(gc);
	unsigned port = off / KTS1620X_PINS;
	unsigned pin = off % KTS1620X_PINS;

	regmap_update_bits(chip->regmap, KTS1620X_OUT_PORT0 + port,
			1u << pin, val ? (1u << pin) : 0);
}

static int kts1620_gpio_get_direction(struct gpio_chip *gc, unsigned off)
{
	struct kts1620_chip *chip = gpiochip_get_data(gc);
	unsigned port = off / KTS1620X_PINS;
	unsigned pin = off % KTS1620X_PINS;
	unsigned int reg;

	regmap_read(chip->regmap, KTS1620X_CONFIG_PORT0 + port, &reg);

	return !!(reg & (1u << pin));
}

static const struct regmap_config kts1620x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct gpio_chip template_chip = {
	.label			= "kts1620x-gpio",
	.owner			= THIS_MODULE,
	.get_direction		= kts1620_gpio_get_direction,
	.direction_input	= kts1620_gpio_direction_input,
	.direction_output	= kts1620_gpio_direction_output,
	.get			= kts1620_gpio_get_value,
	.set			= kts1620_gpio_set_value,
	.base			= -1,
	.can_sleep		= true,
};

static const struct of_device_id kts1620x_of_match_table[] = {
	{
		.compatible = "kinetic,kts1620x-gpio",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, kts1620x_of_match_table);

static int kts1620x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct kts1620_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->gpio_chip = template_chip;
	chip->gpio_chip.label = "kts1620x-gpio";
	chip->gpio_chip.ngpio = KTS1620X_PORTS * KTS1620X_PINS;
	chip->gpio_chip.parent = &client->dev;

	chip->regmap = devm_regmap_init_i2c(client, &kts1620x_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	ret = devm_gpiochip_add_data(&client->dev, &chip->gpio_chip, chip);
	if (ret < 0) {
		dev_err(&client->dev, "Unable to register gpiochip\n");
		return ret;
	}

	i2c_set_clientdata(client, chip);

	return 0;
}

static const struct i2c_device_id kts1620x_id_table[] = {
	{ "kts1620x-gpio", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, kts1620x_id_table);

static struct i2c_driver kts1620x_driver = {
	.driver = {
		.name = "kts1620x-gpio",
		.of_match_table = kts1620x_of_match_table,
	},
	.probe = kts1620x_probe,
	.id_table = kts1620x_id_table,
};
module_i2c_driver(kts1620x_driver);

MODULE_AUTHOR("Andrey Mitrofanov <avmwww@gmail.com>");
MODULE_DESCRIPTION("GPIO expander driver for KTS1620x");
MODULE_LICENSE("GPL");


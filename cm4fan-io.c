/* cm4io_fan.c - Minimal fan control driver for Raspberry Pi CM4 IO Board
 * Controls PWM fan speed based on CPU temperature
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define FAN_POLL_INTERVAL_MS 500

static int min_temp = 50000; // 50.0 C
static int max_temp = 70000; // 70.0 C
module_param(min_temp, int, 0444);
module_param(max_temp, int, 0444);

static struct pwm_device *fan_pwm;
static struct delayed_work fan_work;

static void fan_update(struct work_struct *work)
{
    int temp_mC;
    struct thermal_zone_device *tz;
    struct pwm_state pstate;

    tz = thermal_zone_get_zone_by_name("cpu-thermal");
    if (IS_ERR(tz)) {
        pr_err("cm4io_fan: Failed to get thermal zone\n");
        goto reschedule;
    }

    if (thermal_zone_get_temp(tz, &temp_mC)) {
        pr_err("cm4io_fan: Failed to read temperature\n");
        goto reschedule;
    }

    pwm_get_state(fan_pwm, &pstate);
    pstate.period = 40000; // 25kHz (in nanoseconds)

    if (temp_mC <= min_temp) {
        pstate.duty_cycle = 0;
    } else if (temp_mC >= max_temp) {
        pstate.duty_cycle = pstate.period;
    } else {
        pstate.duty_cycle = (pstate.period * (temp_mC - min_temp)) / (max_temp - min_temp);
    }

    pstate.enabled = true;
    pwm_apply_state(fan_pwm, &pstate);

reschedule:
    schedule_delayed_work(&fan_work, msecs_to_jiffies(FAN_POLL_INTERVAL_MS));
}

static int __init cm4io_fan_init(void)
{
    struct pwm_state pstate;

    fan_pwm = pwm_get(NULL, "pwm0");
    if (IS_ERR(fan_pwm)) {
        pr_err("cm4io_fan: Failed to get PWM device\n");
        return PTR_ERR(fan_pwm);
    }

    pwm_init_state(fan_pwm, &pstate);
    pstate.enabled = true;
    pstate.period = 40000; // 25kHz
    pstate.duty_cycle = 0;
    pwm_apply_state(fan_pwm, &pstate);

    INIT_DELAYED_WORK(&fan_work, fan_update);
    schedule_delayed_work(&fan_work, msecs_to_jiffies(FAN_POLL_INTERVAL_MS));

    pr_info("cm4io_fan: Loaded successfully\n");
    return 0;
}

static void __exit cm4io_fan_exit(void)
{
    cancel_delayed_work_sync(&fan_work);
    pwm_disable(fan_pwm);
    pwm_put(fan_pwm);
    pr_info("cm4io_fan: Unloaded\n");
}

module_init(cm4io_fan_init);
module_exit(cm4io_fan_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Minimal PWM fan driver for CM4 IO Board");

# washingMonitor
Notifies me through Blynk when my washing machine or dryer is done

Based on [Laundry Spy](https://andrewdupont.net/2018/02/15/laundry-spy-part-1-the-hardware/) but made to work with a single MPU6050. My dryer is stacked on top of my washing machine and while the MPU6050 is attached to the back of the latter, it works for both.

Uses the following libraries:
- [Blynk](https://github.com/blynkkk/blynk-library)
- [I2Cdevlib-Core](https://github.com/jrowberg/i2cdevlib)
- [I2Cdevlib-MPU6050](https://github.com/jrowberg/i2cdevlib)
- [WiFiManager](https://github.com/tzapu/WiFiManager)

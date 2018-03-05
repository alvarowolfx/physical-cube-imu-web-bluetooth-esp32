const IMU_SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const ACC_CHARACTERISTIC_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26b8';
const GYRO_CHARACTERISTIC_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';

const COLOR_SERVICE_UUID = '5051269d-7791-4332-b534-c6caab0ae14b';
const COLOR_CHARACTERISTIC_UUID = '3bac26a0-e902-44d5-8820-d8698543147b';

const BLE_MODULE_NAME = 'AwesomeMinecraftBlock';

export default class MinecraftBlock {
  async connect() {
    this.device = await navigator.bluetooth.requestDevice({
      filters: [
        {
          name: BLE_MODULE_NAME
        }
      ],
      optionalServices: [
        IMU_SERVICE_UUID,
        COLOR_SERVICE_UUID,
        COLOR_CHARACTERISTIC_UUID,
        ACC_CHARACTERISTIC_UUID,
        GYRO_CHARACTERISTIC_UUID
      ]
    });

    const server = await this.device.gatt.connect();

    this.imuService = await server.getPrimaryService(IMU_SERVICE_UUID);

    this.accCharacteristic = await this.imuService.getCharacteristic(
      ACC_CHARACTERISTIC_UUID
    );
    console.log('AccCharacteristic', this.accCharacteristic.properties);

    this.gyroCharacteristic = await this.imuService.getCharacteristic(
      GYRO_CHARACTERISTIC_UUID
    );
    console.log('GyroCharacteristic', this.gyroCharacteristic.properties);

    this.colorService = await server.getPrimaryService(COLOR_SERVICE_UUID);

    this.colorCharacteristic = await this.colorService.getCharacteristic(
      COLOR_CHARACTERISTIC_UUID
    );
  }

  disconnect() {
    this.device.gatt.disconnect();
  }

  _readValueToPos(value) {
    let arr = [];
    for (let i = 0; i < value.byteLength; i += 4) {
      let buf = new ArrayBuffer(4);
      let view = new DataView(buf);
      for (let j = 0; j < 4; j++) {
        view.setUint8(j, value.getUint8(i + j));
      }
      let pos = view.getFloat32(0, true);
      arr.push(pos.toFixed(2));
    }
    return {
      x: arr[0],
      y: arr[1],
      z: arr[2]
    };
  }

  async addListener(callback) {
    setInterval(async () => {
      if (this.reading) {
        return;
      }
      this.reading = true;
      const accValue = await this.accCharacteristic.readValue();
      const gyroValue = await this.gyroCharacteristic.readValue();

      const acc = this._readValueToPos(accValue);
      const gyro = this._readValueToPos(gyroValue);

      callback({ acc, gyro });
      this.reading = false;
    }, 50);
  }

  removeListener(callback) {}

  updateColor({ r, g, b }) {
    let data = new Uint8Array([r, g, b]);
    this.colorCharacteristic.writeValue(data);
  }
}

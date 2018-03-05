import React, { Component } from 'react';
import React3 from 'react-three-renderer';
import * as THREE from 'three';
import { SketchPicker } from 'react-color';
import { Throttle } from 'react-throttle';
import './App.css';
import MinecraftBlock from './MinecraftBlock';

class App extends Component {
  state = {
    connected: false,
    selectedColor: { r: 255, b: 0, g: 0 },
    selectedHex: '#FF0000',
    data: {
      acc: { x: 0, y: 0, z: 0 },
      gyro: { x: 0, y: 0, z: 0 }
    },
    block: null,
    cubeRotation: new THREE.Euler(-50, 100, 100)
  };

  constructor(props) {
    super(props);

    this.cameraPosition = new THREE.Vector3(0, 0, 5);
    this.directionalLightPosition = new THREE.Vector3(0, 1, 0);
    this.scenePosition = new THREE.Vector3(0, 0, 0);
  }

  _onAnimate = () => {
    const { data, connected } = this.state;
    if (connected) {
      let { gyro: { x, y, z } } = data;
      const mapAcc = value => -1 * value * Math.PI / 180;
      x = mapAcc(x) * -1;
      y = mapAcc(y);
      z = mapAcc(z);

      this.setState({
        cubeRotation: new THREE.Euler(x, z, y)
      });
    }
  };

  connect = async () => {
    const block = new MinecraftBlock();
    await block.connect();

    block.updateColor(this.state.selectedColor);

    this.setState({
      connected: true,
      block
    });

    block.addListener(data => {
      this.setState({
        data
      });
    });
  };

  onConnectClick = () => {
    if (!this.state.connected) {
      this.connect();
    } else {
      this.state.block && this.state.block.disconnect();
      this.setState({
        connected: false,
        block: null
      });
    }
  };

  sync = () => {
    const { colors, selectedLed, mode, speed } = this.state;
    if (selectedLed === 'all') {
      this.sendData(255, colors[0]);
    } else {
      for (let i = 0; i < colors.length; i++) {
        this.sendData(i, colors[i]);
      }
    }
    this.sendMode(mode);
    this.sendSpeed(speed);
  };

  onToggleAll = () => {
    this.setState({
      selectedLed: 'all'
    });
  };

  handleColorChange = color => {
    const { rgb, hex } = color;
    const { block } = this.state;
    if (block) {
      block.updateColor(rgb);
    }

    this.setState({
      selectedColor: rgb,
      selectedHex: hex
    });
  };

  render() {
    const width = window.innerWidth; // canvas width
    const height = window.innerHeight - 100; // canvas height

    const { connected, selectedColor, data, selectedHex } = this.state;
    return (
      <div className="app">
        <header>
          <h1 className="title">ESP32 + Bluetooth LE + WebBluetooth Demo</h1>
        </header>
        <div className="container">
          <React3
            mainCamera="camera" // this points to the perspectiveCamera which has the name set to "camera" below
            width={width}
            height={height}
            onAnimate={this._onAnimate}
          >
            <resources>
              <texture
                resourceId="texture"
                url="images/redstone.png"
                wrapS={THREE.RepeatWrapping}
                wrapT={THREE.RepeatWrapping}
                anisotropy={16}
              />
              <meshLambertMaterial
                resourceId="material"
                side={THREE.DoubleSide}
              >
                <textureResource resourceId="texture" />
              </meshLambertMaterial>
            </resources>
            <scene>
              <ambientLight color={selectedHex} />
              <directionalLight
                color={0xffffff}
                position={this.directionalLightPosition}
                lookAt={this.scenePosition}
              />
              <perspectiveCamera
                name="camera"
                fov={25}
                aspect={width / height}
                near={0.1}
                far={100}
                position={this.cameraPosition}
              />
              <mesh rotation={this.state.cubeRotation}>
                <boxGeometry width={1} height={1} depth={1} />
                {/* <meshBasicMaterial color={selectedHex} />*/}
                <materialResource resourceId="material" />
              </mesh>
            </scene>
          </React3>
          <div style={{ position: 'absolute', left: 0, top: 100 }}>
            <br />
            <div>
              <button onClick={this.onConnectClick}>
                {connected ? 'Disconnect' : 'Connect'}
              </button>
              <br />
            </div>
            <div className="controls">
              <Throttle time="200" handler="onChange">
                <SketchPicker
                  color={selectedColor}
                  onChange={this.handleColorChange}
                />
              </Throttle>
              <br />
              Accelerometer: <br />
              {JSON.stringify(data.acc)}
              <br />
              Gyroscope: <br /> {JSON.stringify(data.gyro)}
              <br />
            </div>
          </div>
        </div>
      </div>
    );
  }
}

export default App;

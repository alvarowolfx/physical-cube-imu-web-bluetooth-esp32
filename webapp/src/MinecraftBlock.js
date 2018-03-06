import React from 'react';
import * as THREE from 'three';

const MinecraftBlock = ({ rotation }) => {
  return (
    <mesh rotation={rotation}>
      <boxGeometry width={1} height={1} depth={1} />
      {/* <meshBasicMaterial color={selectedHex} />*/}
      <materialResource resourceId="material" />
    </mesh>
  );
};

const MinecraftTexture = () => {
  return (
    <texture
      resourceId="texture"
      url="images/redstone.png"
      wrapS={THREE.RepeatWrapping}
      wrapT={THREE.RepeatWrapping}
      anisotropy={16}
    />
  );
};

const MinecraftMaterial = () => {
  return (
    <meshLambertMaterial resourceId="material" side={THREE.DoubleSide}>
      <textureResource resourceId="texture" />
    </meshLambertMaterial>
  );
};

MinecraftBlock.Texture = MinecraftTexture;
MinecraftBlock.Material = MinecraftMaterial;
export default MinecraftBlock;

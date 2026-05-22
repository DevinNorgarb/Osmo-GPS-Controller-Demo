import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.osmo.phonebtgps',
  appName: 'Osmo BT GPS',
  webDir: 'dist',
  android: {
    allowMixedContent: true,
  },
};

export default config;

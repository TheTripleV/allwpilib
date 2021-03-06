/*----------------------------------------------------------------------------*/
/* Copyright (c) 2019-2020 FIRST. All Rights Reserved.                        */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

package edu.wpi.first.wpilibj.simulation;

import edu.wpi.first.hal.SimBoolean;
import edu.wpi.first.hal.SimDouble;
import edu.wpi.first.hal.SimEnum;
import edu.wpi.first.hal.SimValue;
import edu.wpi.first.hal.simulation.SimDeviceCallback;
import edu.wpi.first.hal.simulation.SimDeviceDataJNI;
import edu.wpi.first.hal.simulation.SimValueCallback;

public class SimDeviceSim {
  private final int m_handle;

  public SimDeviceSim(String name) {
    m_handle = SimDeviceDataJNI.getSimDeviceHandle(name);
  }

  public SimValue getValue(String name) {
    int handle = SimDeviceDataJNI.getSimValueHandle(m_handle, name);
    if (handle <= 0) {
      return null;
    }
    return new SimValue(handle);
  }

  public SimDouble getDouble(String name) {
    int handle = SimDeviceDataJNI.getSimValueHandle(m_handle, name);
    if (handle <= 0) {
      return null;
    }
    return new SimDouble(handle);
  }

  public SimEnum getEnum(String name) {
    int handle = SimDeviceDataJNI.getSimValueHandle(m_handle, name);
    if (handle <= 0) {
      return null;
    }
    return new SimEnum(handle);
  }

  public SimBoolean getBoolean(String name) {
    int handle = SimDeviceDataJNI.getSimValueHandle(m_handle, name);
    if (handle <= 0) {
      return null;
    }
    return new SimBoolean(handle);
  }

  public static String[] getEnumOptions(SimEnum val) {
    return SimDeviceDataJNI.getSimValueEnumOptions(val.getNativeHandle());
  }

  public SimDeviceDataJNI.SimValueInfo[] enumerateValues() {
    return SimDeviceDataJNI.enumerateSimValues(m_handle);
  }

  public int getNativeHandle() {
    return m_handle;
  }

  public CallbackStore registerValueCreatedCallback(SimValueCallback callback, boolean initialNotify) {
    int uid = SimDeviceDataJNI.registerSimValueCreatedCallback(m_handle, callback, initialNotify);
    return new CallbackStore(uid, SimDeviceDataJNI::cancelSimValueCreatedCallback);
  }

  public CallbackStore registerValueChangedCallback(SimValueCallback callback, boolean initialNotify) {
    int uid = SimDeviceDataJNI.registerSimValueChangedCallback(m_handle, callback, initialNotify);
    return new CallbackStore(uid, SimDeviceDataJNI::cancelSimValueChangedCallback);
  }

  public static SimDeviceDataJNI.SimDeviceInfo[] enumerateDevices(String prefix) {
    return SimDeviceDataJNI.enumerateSimDevices(prefix);
  }

  public CallbackStore registerDeviceCreatedCallback(String prefix, SimDeviceCallback callback, boolean initialNotify) {
    int uid = SimDeviceDataJNI.registerSimDeviceCreatedCallback(prefix, callback, initialNotify);
    return new CallbackStore(uid, SimDeviceDataJNI::cancelSimDeviceCreatedCallback);
  }

  public CallbackStore registerDeviceFreedCallback(String prefix, SimDeviceCallback callback) {
    int uid = SimDeviceDataJNI.registerSimDeviceFreedCallback(prefix, callback);
    return new CallbackStore(uid, SimDeviceDataJNI::cancelSimDeviceFreedCallback);
  }

  public static void resetData() {
    SimDeviceDataJNI.resetSimDeviceData();
  }
}

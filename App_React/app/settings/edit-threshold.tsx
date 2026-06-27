import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, TextInput, Switch } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { ChevronLeft, Info } from 'lucide-react-native';
import { useRouter, useLocalSearchParams } from 'expo-router';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useThresholdStore } from '../../src/store/useThresholdStore';
import { useMeshStore } from '../../src/store/useMeshStore';

// Removed Sparkline mock component
export default function EditThresholdScreen() {
  const router = useRouter();
  const params = useLocalSearchParams();
  
  const ip = (params.ip as string) || '192.168.1.101';
  const sensor = (params.sensor as string) || 'BME280';
  const field = (params.field as string) || 'Temperature';
  const key = `${ip}_${sensor}_${field}`;

  const { getThreshold, setThreshold } = useThresholdStore();
  const node = useMeshStore(s => s.nodes[ip]);
  const currentReading = node?.sensorEntries?.find(s => s.sensorName === sensor && s.label === field);
  const currentValue = currentReading ? currentReading.value.toFixed(1) : '--';

  const [enabled, setEnabled] = useState(false);
  const [warnMin, setWarnMin] = useState('18.0');
  const [warnMax, setWarnMax] = useState('32.0');
  const [critMin, setCritMin] = useState('10.0');
  const [critMax, setCritMax] = useState('40.0');
  const [notify, setNotify] = useState(false);

  // Load from SecureStore
  useEffect(() => {
    const config = getThreshold(key);
    setEnabled(config.enabled);
    setWarnMin(config.warnMin);
    setWarnMax(config.warnMax);
    setCritMin(config.critMin);
    setCritMax(config.critMax);
    setNotify(config.notify);
  }, [key, getThreshold]);

  const handleSave = () => {
    setThreshold(key, {
      enabled,
      warnMin,
      warnMax,
      critMin,
      critMax,
      notify
    });
    router.back();
  };

  const getUnit = () => {
    if (field.includes('Temp')) return '°C';
    if (field.includes('Humid')) return '%';
    if (field.includes('Press')) return 'hPa';
    return '';
  };
  const unit = getUnit();

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <TouchableOpacity style={styles.headerButton} onPress={() => router.back()}>
          <ChevronLeft size={24} color={COLORS.textMain} />
        </TouchableOpacity>
        <Text style={styles.headerTitle}>Edit Threshold</Text>
        <TouchableOpacity style={styles.headerButtonRight} onPress={handleSave}>
          <Text style={styles.saveText}>Save</Text>
        </TouchableOpacity>
      </View>
      <Text style={styles.headerSubtitle}>{`${sensor} • ${field}`}</Text>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Current Value Hero */}
        <View style={styles.heroContainer}>
          <View style={styles.heroLeft}>
            <Text style={styles.heroLabel}>Current value</Text>
            <View style={styles.heroValueRow}>
              <Text style={styles.heroValue}>{currentValue}</Text>
              <Text style={styles.heroUnit}>{unit}</Text>
            </View>
          </View>
        </View>

        {/* Form Container */}
        <View style={styles.formContainer}>
          
          {/* Enable Toggle */}
          <View style={styles.formRow}>
            <Text style={styles.formLabel}>Enable threshold</Text>
            <Switch
              value={enabled}
              onValueChange={setEnabled}
              trackColor={{ false: COLORS.surfaceLight, true: COLORS.primary }}
              thumbColor={COLORS.textMain}
            />
          </View>

          <View style={styles.divider} />

          {/* Warning Block */}
          <View style={styles.block}>
            <View style={styles.blockTitleRow}>
              <View style={[styles.dot, { backgroundColor: COLORS.warning }]} />
              <Text style={[styles.blockTitle, { color: COLORS.warning }]}>Warning</Text>
            </View>
            <View style={styles.inputGroupRow}>
              <View style={styles.inputWrapper}>
                <Text style={styles.inputLabel}>Min</Text>
                <View style={styles.inputField}>
                  <TextInput
                    style={styles.textInput}
                    value={warnMin}
                    onChangeText={setWarnMin}
                    keyboardType="numeric"
                    editable={enabled}
                  />
                  <Text style={styles.inputUnit}>{unit}</Text>
                </View>
              </View>
              <View style={styles.inputWrapper}>
                <Text style={styles.inputLabel}>Max</Text>
                <View style={styles.inputField}>
                  <TextInput
                    style={styles.textInput}
                    value={warnMax}
                    onChangeText={setWarnMax}
                    keyboardType="numeric"
                    editable={enabled}
                  />
                  <Text style={styles.inputUnit}>{unit}</Text>
                </View>
              </View>
            </View>
          </View>

          <View style={styles.divider} />

          {/* Critical Block */}
          <View style={styles.block}>
            <View style={styles.blockTitleRow}>
              <View style={[styles.dot, { backgroundColor: COLORS.error }]} />
              <Text style={[styles.blockTitle, { color: COLORS.error }]}>Critical</Text>
            </View>
            <View style={styles.inputGroupRow}>
              <View style={styles.inputWrapper}>
                <Text style={styles.inputLabel}>Min</Text>
                <View style={styles.inputField}>
                  <TextInput
                    style={styles.textInput}
                    value={critMin}
                    onChangeText={setCritMin}
                    keyboardType="numeric"
                    editable={enabled}
                  />
                  <Text style={styles.inputUnit}>{unit}</Text>
                </View>
              </View>
              <View style={styles.inputWrapper}>
                <Text style={styles.inputLabel}>Max</Text>
                <View style={styles.inputField}>
                  <TextInput
                    style={styles.textInput}
                    value={critMax}
                    onChangeText={setCritMax}
                    keyboardType="numeric"
                    editable={enabled}
                  />
                  <Text style={styles.inputUnit}>{unit}</Text>
                </View>
              </View>
            </View>
          </View>

          <View style={styles.divider} />

          {/* Alert Toggle */}
          <View style={styles.formRow}>
            <View>
              <Text style={styles.formLabel}>Alert</Text>
              <Text style={styles.formSubtitle}>Notify when out of range</Text>
            </View>
            <Switch
              value={notify}
              onValueChange={setNotify}
              trackColor={{ false: COLORS.surfaceLight, true: COLORS.primary }}
              thumbColor={COLORS.textMain}
              disabled={!enabled}
            />
          </View>

        </View>

        {/* Info Message */}
        <View style={styles.infoBox}>
          <Info size={20} color={COLORS.secondary} style={styles.infoIcon} />
          <Text style={styles.infoText}>
            You will receive alerts when the value is outside the critical range.
          </Text>
        </View>

      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: SIZES.large,
    paddingTop: SIZES.medium,
  },
  headerButton: {
    width: 40,
    height: 40,
    justifyContent: 'center',
    alignItems: 'flex-start',
  },
  headerTitle: {
    fontSize: SIZES.large,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  headerButtonRight: {
    width: 40,
    height: 40,
    justifyContent: 'center',
    alignItems: 'flex-end',
  },
  saveText: {
    color: COLORS.info, // Blue text
    fontSize: SIZES.font,
    fontWeight: '600',
  },
  headerSubtitle: {
    textAlign: 'center',
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: SIZES.large,
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  
  // Hero
  heroContainer: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: SIZES.large,
  },
  heroLeft: {
    flex: 1,
  },
  heroLabel: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: 4,
  },
  heroValueRow: {
    flexDirection: 'row',
    alignItems: 'flex-start',
  },
  heroValue: {
    color: COLORS.textMain,
    fontSize: 32,
    fontWeight: 'bold',
  },
  heroUnit: {
    color: COLORS.secondary,
    fontSize: SIZES.font,
    marginTop: 6,
    marginLeft: 2,
  },

  // Form Container
  formContainer: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    borderWidth: 1,
    borderColor: COLORS.border,
    padding: SIZES.large,
  },
  formRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: SIZES.small,
  },
  formLabel: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: '500',
  },
  formSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginTop: 2,
  },
  divider: {
    height: 1,
    backgroundColor: 'rgba(255,255,255,0.05)',
    marginVertical: SIZES.medium,
  },

  // Blocks
  block: {
    paddingVertical: 4,
  },
  blockTitleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: SIZES.medium,
  },
  dot: {
    width: 6,
    height: 6,
    borderRadius: 3,
  },
  blockTitle: {
    fontSize: SIZES.font,
    fontWeight: 'bold',
  },
  inputGroupRow: {
    flexDirection: 'row',
    gap: SIZES.large,
  },
  inputWrapper: {
    flex: 1,
  },
  inputLabel: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: 8,
  },
  inputField: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  textInput: {
    flex: 1,
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
    paddingVertical: 4,
  },
  inputUnit: {
    color: COLORS.secondary,
    fontSize: SIZES.font,
    marginLeft: 8,
  },

  // Info Box
  infoBox: {
    flexDirection: 'row',
    backgroundColor: 'rgba(255,255,255,0.03)',
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
    marginTop: SIZES.large,
    alignItems: 'flex-start',
    gap: 12,
  },
  infoIcon: {
    marginTop: 2,
  },
  infoText: {
    flex: 1,
    color: COLORS.secondary,
    fontSize: 13,
    lineHeight: 18,
  }
});

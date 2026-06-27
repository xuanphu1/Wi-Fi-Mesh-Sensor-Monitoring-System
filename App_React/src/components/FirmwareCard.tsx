import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { Cpu, CloudDownload } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface FirmwareCardProps {
  title: string;
  version: string;
  releaseTime: string;
  size: string;
  iconColor: string;
  isLatest?: boolean;
  onDownload?: () => void;
}

export default function FirmwareCard({ 
  title, 
  version, 
  releaseTime, 
  size, 
  iconColor, 
  isLatest, 
  onDownload 
}: FirmwareCardProps) {
  return (
    <TouchableOpacity 
      style={styles.card} 
      activeOpacity={0.7}
      onPress={onDownload}
    >
      <View style={styles.content}>
        <View style={[styles.iconContainer, { backgroundColor: `${iconColor}20` }]}>
          <Cpu size={24} color={iconColor} />
        </View>
        
        <View style={styles.textContainer}>
          <View style={styles.titleRow}>
            <Text style={styles.title}>{title}</Text>
            {isLatest && (
              <View style={styles.latestBadge}>
                <Text style={styles.latestText}>Latest</Text>
              </View>
            )}
          </View>
          <View style={styles.subtitleRow}>
            <Text style={styles.subtitle}>{version}</Text>
            <Text style={styles.dot}>•</Text>
            <Text style={styles.subtitle}>{releaseTime}</Text>
          </View>
        </View>

        <View style={styles.rightAction}>
          <CloudDownload size={24} color={COLORS.info} />
          <Text style={styles.sizeText}>{size}</Text>
        </View>
      </View>
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    marginBottom: SIZES.medium,
    padding: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  content: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  iconContainer: {
    width: 48,
    height: 48,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  textContainer: {
    flex: 1,
    justifyContent: 'center',
  },
  titleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 4,
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  latestBadge: {
    backgroundColor: 'rgba(34, 197, 94, 0.15)', // Success green transparent
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 4,
  },
  latestText: {
    color: COLORS.success,
    fontSize: 10,
    fontWeight: 'bold',
  },
  subtitleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  subtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  dot: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  rightAction: {
    justifyContent: 'center',
    alignItems: 'center',
    paddingLeft: SIZES.small,
    gap: 4,
  },
  sizeText: {
    color: COLORS.secondary,
    fontSize: 10,
  }
});

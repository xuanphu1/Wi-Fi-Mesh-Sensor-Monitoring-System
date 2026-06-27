import React, { useEffect, useRef } from 'react';
import { Animated, Text, StyleSheet, View, TouchableOpacity } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { Info, CheckCircle, AlertTriangle, XCircle, X } from 'lucide-react-native';
import { useToastStore, ToastType } from '../store/useToastStore';
import { COLORS, SIZES } from '../constants/theme';

const TOAST_DURATION = 5000; // 5 seconds

export default function GlobalToast() {
  const { visible, title, message, type, hideToast } = useToastStore();
  const insets = useSafeAreaInsets();
  const translateY = useRef(new Animated.Value(-150)).current;
  const opacity = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    let timeout: NodeJS.Timeout;

    if (visible) {
      Animated.parallel([
        Animated.timing(translateY, {
          toValue: insets.top > 0 ? insets.top + 10 : 20,
          duration: 350,
          useNativeDriver: true,
        }),
        Animated.timing(opacity, {
          toValue: 1,
          duration: 350,
          useNativeDriver: true,
        })
      ]).start();

      timeout = setTimeout(() => {
        hideToast();
      }, TOAST_DURATION);

    } else {
      Animated.parallel([
        Animated.timing(translateY, {
          toValue: -150,
          duration: 300,
          useNativeDriver: true,
        }),
        Animated.timing(opacity, {
          toValue: 0,
          duration: 300,
          useNativeDriver: true,
        })
      ]).start();
    }

    return () => clearTimeout(timeout);
  }, [visible, insets.top]);

  const getToastStyle = (t: ToastType) => {
    switch (t) {
      case 'success':
        return { color: COLORS.success, icon: <CheckCircle size={24} color={COLORS.success} /> };
      case 'warning':
        return { color: COLORS.warning, icon: <AlertTriangle size={24} color={COLORS.warning} /> };
      case 'error':
        return { color: COLORS.error, icon: <XCircle size={24} color={COLORS.error} /> };
      case 'info':
      default:
        return { color: COLORS.info, icon: <Info size={24} color={COLORS.info} /> };
    }
  };

  const styleConfig = getToastStyle(type);

  return (
    <Animated.View
      style={[
        styles.container,
        {
          borderColor: styleConfig.color,
          transform: [{ translateY }],
          opacity,
          shadowColor: styleConfig.color,
        }
      ]}
      pointerEvents={visible ? 'auto' : 'none'}
    >
      <View style={styles.content}>
        <View style={styles.iconContainer}>{styleConfig.icon}</View>
        <View style={styles.textContainer}>
          <Text style={styles.title}>{title}</Text>
          <Text style={styles.message}>{message}</Text>
        </View>
        <TouchableOpacity style={styles.closeButton} onPress={hideToast}>
          <X size={20} color={COLORS.secondary} />
        </TouchableOpacity>
      </View>
      
      {/* Small progress bar at bottom to show duration */}
      {visible && (
        <View style={styles.progressTrack}>
           <Animated.View 
             style={[
               styles.progressBar, 
               { backgroundColor: styleConfig.color }
             ]} 
           />
        </View>
      )}
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    top: 0,
    left: SIZES.large,
    right: SIZES.large,
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    borderWidth: 1,
    borderLeftWidth: 4,
    zIndex: 9999,
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 10,
    elevation: 8,
    overflow: 'hidden', // for progress bar
  },
  content: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: SIZES.medium,
  },
  iconContainer: {
    marginRight: SIZES.medium,
  },
  textContainer: {
    flex: 1,
    marginRight: SIZES.small,
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.font,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  message: {
    color: COLORS.secondary,
    fontSize: 13,
    lineHeight: 18,
  },
  closeButton: {
    padding: 4,
  },
  progressTrack: {
    height: 3,
    backgroundColor: 'rgba(255,255,255,0.05)',
    width: '100%',
  },
  progressBar: {
    height: '100%',
    width: '100%', // Could animate this too, but leaving simple for now
  }
});

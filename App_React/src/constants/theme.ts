export const COLORS = {
  // Vision UI Dark Theme Colors
  background: "#080c21", // Deep dark blue background
  surface: "#101633", // Main card background
  surfaceLight: "#182042", // Lighter card background
  glassBackground: "rgba(16, 22, 51, 0.76)",
  glassBorder: "rgba(50, 105, 255, 0.35)", // Card border
  border: "#202852",
  
  // Accents
  primary: "#0075ff", // Bright blue
  primaryNeon: "rgba(0, 117, 255, 0.8)",
  secondary: "#8fa7d6", // Muted text
  success: "#01f7a7", // Bright green
  successDark: "rgba(1, 247, 167, 0.15)",
  warning: "#ffb547", // Orange/Yellow
  warningDark: "rgba(255, 181, 71, 0.15)",
  error: "#ff285c", // Bright red
  info: "#8a2be2", // Purple for nodes
  
  // Typography
  textMain: "#ffffff",
  textMuted: "#a0aec0", // Light gray for secondary text
};

export const SIZES = {
  base: 8,
  small: 12,
  font: 14,
  medium: 16,
  large: 18,
  extraLarge: 24,
  radius: 16,
  cardRadius: 20,
};

export const SHADOWS = {
  glowPrimary: {
    shadowColor: COLORS.primary,
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 10,
    elevation: 8,
  },
  glowSuccess: {
    shadowColor: COLORS.success,
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 10,
    elevation: 8,
  },
};

// Legacy exports for backward compatibility with existing UI components
export const colors = {
  bg: '#F7F9FC',
  surface: '#FFFFFF',
  surfaceMuted: '#EEF3F8',
  text: '#172033',
  muted: '#667085',
  border: '#DDE5EE',
  primary: '#116466',
  primarySoft: '#DDF3F1',
  accent: '#F59E0B',
  success: '#178C55',
  danger: '#D14343',
  info: '#2563EB',
};

export const spacing = {
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 24,
};


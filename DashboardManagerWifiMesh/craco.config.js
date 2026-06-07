/**
 * stylis-plugin-rtl (và vài gói khác) kèm source map trỏ tới file .ts không có trong npm — webpack báo ENOENT.
 * Bỏ qua cảnh báo đó (không ảnh hưởng chạy app).
 */
module.exports = {
  webpack: {
    configure: (webpackConfig) => {
      webpackConfig.ignoreWarnings = [
        ...(webpackConfig.ignoreWarnings || []),
        /Failed to parse source map/,
      ];
      return webpackConfig;
    },
  },
};

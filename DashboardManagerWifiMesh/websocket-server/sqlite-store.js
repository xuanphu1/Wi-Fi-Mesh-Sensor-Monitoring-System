const path = require("path");
const sqlite3 = require("sqlite3");
const { open } = require("sqlite");

function looksLikeMeshUdpJson(o) {
  return (
    o &&
    typeof o === "object" &&
    typeof o.v === "number" &&
    typeof o.n === "number" &&
    (typeof o.i === "string" || typeof o.M === "string") &&
    Array.isArray(o.p)
  );
}

function createSqliteStore({ dbPath, sensorValueFieldNames, sensorTypeName }) {
  let db = null;
  let connectingPromise = null;
  const resolvedPath = path.isAbsolute(dbPath || "")
    ? dbPath
    : path.join(__dirname, dbPath || "mesh-data.sqlite");

  async function connect() {
    if (db) return db;
    if (connectingPromise) return connectingPromise;

    connectingPromise = (async () => {
      const database = await open({
        filename: resolvedPath,
        driver: sqlite3.Database,
      });

      // Enable Write-Ahead Logging for better concurrency
      await database.exec("PRAGMA journal_mode = WAL;");

      await database.exec(`
        CREATE TABLE IF NOT EXISTS Data (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          mac TEXT NOT NULL,
          deviceTime TEXT,
          serverTime TEXT NOT NULL,
          meshLevel INTEGER,
          schemaVersion INTEGER,
          packetLoss REAL,
          port INTEGER,
          sensorType INTEGER,
          sensorName TEXT,
          field TEXT NOT NULL,
          value REAL NOT NULL,
          nodeTimeValid INTEGER,
          sourceIp TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_data_time ON Data(serverTime DESC);
        CREATE INDEX IF NOT EXISTS idx_data_mac_field_time ON Data(mac, field, serverTime);

        CREATE TABLE IF NOT EXISTS Firmwares (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          targetType TEXT NOT NULL,
          version TEXT NOT NULL,
          filename TEXT NOT NULL UNIQUE,
          originalName TEXT,
          fileSize INTEGER NOT NULL,
          checksum TEXT,
          bin BLOB,
          channel TEXT DEFAULT 'stable',
          notes TEXT,
          uploadedAt TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_firmwares_type ON Firmwares(targetType, uploadedAt DESC);

        CREATE TABLE IF NOT EXISTS OtaJobs (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          jobId TEXT NOT NULL UNIQUE,
          targetType TEXT NOT NULL,
          targetDetail TEXT,
          targetMac TEXT,
          version TEXT NOT NULL,
          firmwareUrl TEXT NOT NULL,
          fileSize INTEGER,
          checksum TEXT,
          status TEXT NOT NULL,
          progress INTEGER DEFAULT 0,
          summary TEXT,
          errorMsg TEXT,
          startedAt TEXT NOT NULL,
          completedAt TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_otajobs_time ON OtaJobs(startedAt DESC);
      `);

      // Auto-migrate tables if created with older schema versions
      try {
        const fwTableInfo = await database.all("PRAGMA table_info(Firmwares)");
        const hasBin = fwTableInfo.some((col) => col.name === "bin");
        if (!hasBin && fwTableInfo.length > 0) {
          await database.run("ALTER TABLE Firmwares ADD COLUMN bin BLOB");
          console.log("[db] Auto-migrated: Added 'bin' BLOB column to Firmwares table in SQLite");
        }

        const otaTableInfo = await database.all("PRAGMA table_info(OtaJobs)");
        const hasTargetDetail = otaTableInfo.some((col) => col.name === "targetDetail");
        if (!hasTargetDetail && otaTableInfo.length > 0) {
          await database.run("ALTER TABLE OtaJobs ADD COLUMN targetDetail TEXT");
          await database.run("UPDATE OtaJobs SET targetDetail = targetMac WHERE targetDetail IS NULL");
          console.log("[db] Auto-migrated: Added 'targetDetail' column to OtaJobs table in SQLite");
        }
      } catch (e) {
        console.warn("[db] Migration check warning:", e.message);
      }

      db = database;
      return database;
    })();

    try {
      return await connectingPromise;
    } finally {
      connectingPromise = null;
    }
  }

  async function ensureConnected() {
    if (!db) {
      await connect();
    }
    return db;
  }

  function buildMeshRows(pkt, sourceIp, receivedAt) {
    if (!looksLikeMeshUdpJson(pkt)) return [];
    const serverTime = receivedAt.toISOString();
    const deviceTime = pkt && typeof pkt.t === "string" ? pkt.t.trim() : null;
    const meshLevel = Number(pkt.n);
    const schemaVersion = Number(pkt.v);
    const packetLossRaw = pkt.packetloss ?? pkt.packetLoss;
    const packetLoss =
      packetLossRaw !== undefined && packetLossRaw !== null && packetLossRaw !== ""
        ? Number(packetLossRaw)
        : null;
    const mac = String(pkt.M || pkt.i || "");
    if (!mac || mac === "0.0.0.0") return [];
    const rows = [];

    pkt.p.forEach((row) => {
      if (!Array.isArray(row) || row.length < 3) return;
      const port = Number(row[0]);
      const sensorType = Number(row[1]);
      const sensorName = sensorTypeName(sensorType);
      const values = row.slice(2);
      const labels = sensorValueFieldNames[sensorType] || [];
      values.forEach((cell, idx) => {
        const value = Number(cell);
        if (!Number.isFinite(value)) return;
        const field = labels[idx] || `v${idx}`;
        rows.push({
          mac,
          deviceTime,
          serverTime,
          meshLevel,
          schemaVersion,
          packetLoss: Number.isFinite(packetLoss) ? packetLoss : null,
          port,
          sensorType,
          sensorName,
          field,
          value,
          nodeTimeValid: 0,
          sourceIp,
        });
      });
    });

    if (Number.isFinite(packetLoss)) {
      rows.push({
        mac,
        deviceTime,
        serverTime,
        meshLevel,
        schemaVersion,
        packetLoss,
        port: 0,
        sensorType: -1,
        sensorName: "System",
        field: "packetloss",
        value: packetLoss,
        nodeTimeValid: 0,
        sourceIp,
      });
    }

    return rows;
  }

  async function persistMeshPacket(pkt, sourceIp, receivedAt = new Date()) {
    const activeDb = await ensureConnected();
    const rows = buildMeshRows(pkt, sourceIp, receivedAt);
    if (!rows.length) return 0;

    const placeholders = rows.map(() => "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").join(", ");
    const values = [];
    rows.forEach((r) => {
      values.push(
        r.mac,
        r.deviceTime,
        r.serverTime,
        r.meshLevel,
        r.schemaVersion,
        r.packetLoss,
        r.port,
        r.sensorType,
        r.sensorName,
        r.field,
        r.value,
        r.nodeTimeValid,
        r.sourceIp
      );
    });

    const query = `
      INSERT INTO Data (
        mac, deviceTime, serverTime, meshLevel, 
        schemaVersion, packetLoss, port, sensorType, sensorName, 
        field, value, nodeTimeValid, sourceIp
      ) VALUES ${placeholders}
    `;

    try {
      await activeDb.run(query, values);
      return rows.length;
    } catch (err) {
      console.error("[sqlite] insert failed:", err.message);
      return 0;
    }
  }

  async function persistGatewayStatus(pkt, sourceIp, receivedAt = new Date()) {
    if (!pkt || pkt.type !== "gateway_status") return 0;
    const mac = pkt.mac || pkt.sta_mac || pkt.sta_ip || sourceIp || "gateway";
    const serverTime = receivedAt.toISOString();
    
    const metrics = {
      cpu_load: pkt.cpu_load_percent,
      ram_used: pkt.ram_used_percent,
      wifi_rssi: pkt.wifi_rssi,
      battery_v: pkt.battery_voltage_v,
      uptime_s: pkt.uptime_s
    };

    const rows = [];
    for (const [field, val] of Object.entries(metrics)) {
      const num = Number(val);
      if (Number.isFinite(num)) {
        rows.push({
          mac,
          deviceTime: null,
          serverTime,
          meshLevel: -1,
          schemaVersion: -1,
          packetLoss: null,
          port: -1,
          sensorType: -1,
          sensorName: "Gateway",
          field,
          value: num,
          nodeTimeValid: 0,
          sourceIp
        });
      }
    }

    if (!rows.length) return 0;

    const placeholders = rows.map(() => "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").join(", ");
    const values = [];
    rows.forEach((r) => {
      values.push(
        r.mac, r.deviceTime, r.serverTime, r.meshLevel, r.schemaVersion,
        r.packetLoss, r.port, r.sensorType, r.sensorName, r.field,
        r.value, r.nodeTimeValid, r.sourceIp
      );
    });

    const query = `
      INSERT INTO Data (
        mac, deviceTime, serverTime, meshLevel, 
        schemaVersion, packetLoss, port, sensorType, 
        sensorName, field, value, nodeTimeValid, sourceIp
      )
      VALUES ${placeholders}
    `;

    try {
      const activeDb = await ensureConnected();
      await activeDb.run(query, values);
      return rows.length;
    } catch (err) {
      console.error("[sqlite] persistGatewayStatus failed:", err.message);
      return 0;
    }
  }

  async function getNodeSnapshot() {
    const activeDb = await ensureConnected();
    const rows = await activeDb.all(`
      SELECT 
        mac as id,
        mac,
        MAX(serverTime) as lastSeen,
        meshLevel,
        schemaVersion,
        packetLoss
      FROM Data
      GROUP BY mac
      ORDER BY mac ASC
    `);

    // SQLite returns ISO strings, we should try to match MongoDB output format if possible, 
    // but the frontend parses standard ISO dates anyway.
    return rows.map(r => ({
      ...r,
      lastSeen: new Date(r.lastSeen)
    }));
  }

  async function getHistoryMacs(limit = 200) {
    const activeDb = await ensureConnected();
    const n = Math.max(1, Math.min(Number(limit) || 200, 2000));
    const rows = await activeDb.all(`
      SELECT mac, MAX(serverTime) as lastSeen, GROUP_CONCAT(DISTINCT sensorName) as sensors
      FROM Data
      GROUP BY mac
      ORDER BY lastSeen DESC
      LIMIT ?
    `, [n]);

    return rows.map(r => ({
      mac: r.mac,
      lastSeen: new Date(r.lastSeen),
      sensors: r.sensors ? r.sensors.split(",") : []
    }));
  }

  async function getHistorySeries({ mac, sensorName, field, from, to, limit = 2000 }) {
    const activeDb = await ensureConnected();
    const macNorm = String(mac || "").trim();
    const sensorNameNorm = String(sensorName || "").trim();
    const fieldNorm = String(field || "").trim();
    if (!macNorm || !fieldNorm) return [];
    const max = Math.max(1, Math.min(Number(limit) || 2000, 5000));

    let query = `SELECT mac, sensorName, field, serverTime as time, value FROM Data WHERE mac = ? AND field = ?`;
    const params = [macNorm, fieldNorm];

    if (sensorNameNorm) {
      query += ` AND sensorName = ?`;
      params.push(sensorNameNorm);
    }

    if (from) {
      const d = new Date(from);
      if (Number.isFinite(d.getTime())) {
        query += ` AND serverTime >= ?`;
        params.push(d.toISOString());
      }
    }
    if (to) {
      const d = new Date(to);
      if (Number.isFinite(d.getTime())) {
        query += ` AND serverTime <= ?`;
        params.push(d.toISOString());
      }
    }

    if (from) {
      query += ` ORDER BY serverTime ASC LIMIT ?`;
      params.push(max);
      const rows = await activeDb.all(query, params);
      return rows.map(r => ({
        mac: r.mac,
        field: r.field,
        value: r.value,
        time: new Date(r.time)
      }));
    } else {
      query += ` ORDER BY serverTime DESC LIMIT ?`;
      params.push(max);
      const rows = await activeDb.all(query, params);
      rows.reverse();
      return rows.map(r => ({
        mac: r.mac,
        field: r.field,
        value: r.value,
        time: new Date(r.time)
      }));
    }

  }

  function toLocalTimeStr(isoString) {
    if (!isoString) return "";
    const d = new Date(isoString);
    if (!Number.isFinite(d.getTime())) return isoString;
    const pad = n => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
  }

  async function getHistoryAllForMacCsv(mac, from, to, onProgress, res) {
    const activeDb = await ensureConnected();
    const macNorm = String(mac || "").trim();
    const header = "DeviceTime,ServerTime,MeshLevel,PacketLoss,Port,Sensor,Field,Value\n";
    
    if (!macNorm) {
      if (res) { res.write(header); res.end(); return; }
      return header;
    }

    let countQuery = `SELECT COUNT(*) as c FROM Data WHERE mac = ?`;
    let query = `SELECT deviceTime, serverTime, meshLevel, packetLoss, port, sensorName, field, value FROM Data WHERE mac = ?`;
    const params = [macNorm];

    if (from) {
      const d = new Date(from);
      if (Number.isFinite(d.getTime())) {
        countQuery += ` AND serverTime >= ?`;
        query += ` AND serverTime >= ?`;
        params.push(d.toISOString());
      }
    }
    if (to) {
      const d = new Date(to);
      if (Number.isFinite(d.getTime())) {
        countQuery += ` AND serverTime <= ?`;
        query += ` AND serverTime <= ?`;
        params.push(d.toISOString());
      }
    }

    const countRow = await activeDb.get(countQuery, params);
    const total = countRow ? countRow.c : 0;

    query += ` ORDER BY serverTime ASC`;

    if (res) {
      res.write(header);
    }

    let csvAccumulator = res ? null : header;
    
    if (total === 0) {
      if (res) { res.end(); return; }
      return csvAccumulator;
    }

    const limit = 5000;
    let offset = 0;

    while (offset < total) {
      const chunkQuery = query + ` LIMIT ${limit} OFFSET ${offset}`;
      const rows = await db.all(chunkQuery, params);
      
      if (rows.length === 0) break;

      let chunkCsv = "";
      rows.forEach(r => {
        const dt = toLocalTimeStr(r.deviceTime);
        const st = toLocalTimeStr(r.serverTime);
        chunkCsv += `${dt},${st},${r.meshLevel || ""},${r.packetLoss || 0},${r.port || 0},${r.sensorName || ""},${r.field || ""},${r.value || 0}\n`;
      });

      if (res) {
        // Write to stream
        res.write(chunkCsv);
      } else {
        // Accumulate in memory
        csvAccumulator += chunkCsv;
      }

      offset += rows.length;
      if (onProgress) onProgress(offset, total);
    }

    if (res) {
      res.end();
      return;
    }
    return csvAccumulator;
  }

  // --- Firmware & OTA Job Methods ---

  async function saveFirmware({ targetType, version, filename, originalName, fileSize, checksum, bin, channel = "stable", notes = "" }) {
    const activeDb = await ensureConnected();
    const uploadedAt = new Date().toISOString();
    
    // Check if duplicate filename exists, replace or insert
    const existing = await activeDb.get("SELECT id FROM Firmwares WHERE filename = ?", [filename]);
    if (existing) {
      await activeDb.run(
        `UPDATE Firmwares SET targetType = ?, version = ?, originalName = ?, fileSize = ?, checksum = ?, bin = ?, channel = ?, notes = ?, uploadedAt = ? WHERE id = ?`,
        [targetType, version, originalName, fileSize, checksum, bin, channel, notes, uploadedAt, existing.id]
      );
      return { id: existing.id, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt };
    }

    const result = await activeDb.run(
      `INSERT INTO Firmwares (targetType, version, filename, originalName, fileSize, checksum, bin, channel, notes, uploadedAt) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      [targetType, version, filename, originalName, fileSize, checksum, bin, channel, notes, uploadedAt]
    );
    return { id: result.lastID, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt };
  }

  async function getFirmwares(targetType = null) {
    const activeDb = await ensureConnected();
    // Exclude 'bin' column for light & fast UI listing
    if (targetType) {
      return await activeDb.all("SELECT id, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt FROM Firmwares WHERE targetType = ? ORDER BY uploadedAt DESC", [targetType]);
    }
    return await activeDb.all("SELECT id, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt FROM Firmwares ORDER BY uploadedAt DESC");
  }

  async function getFirmwareById(id) {
    const activeDb = await ensureConnected();
    return await activeDb.get("SELECT id, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt FROM Firmwares WHERE id = ?", [id]);
  }

  async function getFirmwareByFilename(filename) {
    const activeDb = await ensureConnected();
    return await activeDb.get("SELECT id, targetType, version, filename, originalName, fileSize, checksum, channel, notes, uploadedAt FROM Firmwares WHERE filename = ? COLLATE NOCASE", [filename]);
  }

  async function getFirmwareBinary(filename) {
    const activeDb = await ensureConnected();
    const fw = await activeDb.get("SELECT id, filename, version, fileSize, checksum, bin FROM Firmwares WHERE filename = ? COLLATE NOCASE", [filename]);
    if (!fw) return null;

    // Fallback: If bin is NULL (uploaded before BLOB migration), check if file is on server disk and auto-repair
    if (!fw.bin) {
      const possiblePaths = [
        path.join(__dirname, "uploads", "firmware", filename),
        path.join(__dirname, "uploads", filename),
      ];
      for (const p of possiblePaths) {
        if (fs.existsSync(p)) {
          try {
            const buf = fs.readFileSync(p);
            await activeDb.run("UPDATE Firmwares SET bin = ? WHERE id = ?", [buf, fw.id]);
            fw.bin = buf;
            console.log(`[db] Auto-repaired firmware binary in SQLite from disk: ${filename}`);
            break;
          } catch (e) {}
        }
      }
    }
    return fw;
  }

  async function deleteFirmware(id) {
    const activeDb = await ensureConnected();
    const item = await activeDb.get("SELECT id, filename FROM Firmwares WHERE id = ?", [id]);
    if (item) {
      await activeDb.run("DELETE FROM Firmwares WHERE id = ?", [id]);
    }
    return item;
  }

  async function createOtaJob({ jobId, targetType, targetDetail, targetMac = "all", version, firmwareUrl, fileSize = 0, checksum = "", summary = "" }) {
    const activeDb = await ensureConnected();
    const startedAt = new Date().toISOString();
    const status = "Initiated";
    const progress = 0;
    const finalTargetDetail = targetDetail || targetMac || (targetType === "gateway" ? "gateway" : "all");

    await activeDb.run(
      `INSERT INTO OtaJobs (jobId, targetType, targetDetail, targetMac, version, firmwareUrl, fileSize, checksum, status, progress, summary, startedAt) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      [jobId, targetType, finalTargetDetail, finalTargetDetail, version, firmwareUrl, fileSize, checksum, status, progress, summary, startedAt]
    );

    return { jobId, targetType, targetDetail: finalTargetDetail, targetMac: finalTargetDetail, version, firmwareUrl, fileSize, checksum, status, progress, summary, startedAt };
  }

  async function updateOtaJobProgress(jobId, { status, progress, summary, errorMsg, completedAt }) {
    const activeDb = await ensureConnected();
    const updates = [];
    const params = [];

    if (status !== undefined) {
      updates.push("status = ?");
      params.push(status);
    }
    if (progress !== undefined) {
      updates.push("progress = ?");
      params.push(progress);
    }
    if (summary !== undefined) {
      updates.push("summary = ?");
      params.push(summary);
    }
    if (errorMsg !== undefined) {
      updates.push("errorMsg = ?");
      params.push(errorMsg);
    }
    if (completedAt !== undefined) {
      updates.push("completedAt = ?");
      params.push(completedAt);
    }

    if (updates.length > 0) {
      params.push(jobId);
      await activeDb.run(`UPDATE OtaJobs SET ${updates.join(", ")} WHERE jobId = ?`, params);
    }

    return await activeDb.get("SELECT * FROM OtaJobs WHERE jobId = ?", [jobId]);
  }

  async function getOtaJobs(targetType = null, limit = 50) {
    const activeDb = await ensureConnected();
    const max = Math.max(1, Math.min(Number(limit) || 50, 200));
    if (targetType) {
      return await activeDb.all("SELECT * FROM OtaJobs WHERE targetType = ? ORDER BY startedAt DESC LIMIT ?", [targetType, max]);
    }
    return await activeDb.all("SELECT * FROM OtaJobs ORDER BY startedAt DESC LIMIT ?", [max]);
  }

  return {
    connect,
    persistMeshPacket,
    persistGatewayStatus,
    getNodeSnapshot,
    getHistoryMacs,
    getHistorySeries,
    getHistoryAllForMacCsv,
    isMeshPayload: looksLikeMeshUdpJson,
    saveFirmware,
    getFirmwares,
    getFirmwareById,
    getFirmwareByFilename,
    getFirmwareBinary,
    deleteFirmware,
    createOtaJob,
    updateOtaJobProgress,
    getOtaJobs,
  };
}

module.exports = {
  createSqliteStore,
};

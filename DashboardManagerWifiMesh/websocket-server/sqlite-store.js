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

        CREATE TABLE IF NOT EXISTS Gateways (
          gw_id TEXT PRIMARY KEY,
          name TEXT NOT NULL,
          location TEXT DEFAULT '',
          latitude REAL DEFAULT NULL,
          longitude REAL DEFAULT NULL,
          root_mac TEXT DEFAULT '',
          mesh_id TEXT DEFAULT '',
          ip TEXT DEFAULT '',
          status TEXT DEFAULT 'online',
          last_seen TEXT NOT NULL,
          node_count INTEGER DEFAULT 0,
          notes TEXT DEFAULT ''
        );
        CREATE INDEX IF NOT EXISTS idx_gateways_status ON Gateways(status, last_seen DESC);

        CREATE TABLE IF NOT EXISTS MeshNetworks (
          mesh_id TEXT PRIMARY KEY,
          gw_id TEXT NOT NULL,
          name TEXT NOT NULL,
          channel INTEGER DEFAULT 1,
          node_count INTEGER DEFAULT 0,
          created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS NodesMeta (
          mac TEXT PRIMARY KEY,
          name TEXT DEFAULT '',
          location TEXT DEFAULT '',
          latitude REAL DEFAULT NULL,
          longitude REAL DEFAULT NULL,
          notes TEXT DEFAULT '',
          updated_at TEXT NOT NULL
        );
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

        // Migrate Gateways table for latitude and longitude
        const gwTableInfo = await database.all("PRAGMA table_info(Gateways)");
        const hasLat = gwTableInfo.some((col) => col.name === "latitude");
        if (!hasLat && gwTableInfo.length > 0) {
          await database.run("ALTER TABLE Gateways ADD COLUMN latitude REAL DEFAULT NULL");
          await database.run("ALTER TABLE Gateways ADD COLUMN longitude REAL DEFAULT NULL");
          console.log("[db] Auto-migrated: Added 'latitude' and 'longitude' columns to Gateways table");
        }

        // Migrate Data table for Multi-Gateway support (gw_id, mesh_id)
        const dataTableInfo = await database.all("PRAGMA table_info(Data)");
        const hasGwId = dataTableInfo.some((col) => col.name === "gw_id");
        if (!hasGwId && dataTableInfo.length > 0) {
          await database.run("ALTER TABLE Data ADD COLUMN gw_id TEXT DEFAULT 'gw-default'");
          await database.run("ALTER TABLE Data ADD COLUMN mesh_id TEXT DEFAULT 'mesh-default'");
          await database.run("CREATE INDEX IF NOT EXISTS idx_data_gw_mesh ON Data(gw_id, mesh_id, serverTime DESC)");
          console.log("[db] Auto-migrated: Added 'gw_id' and 'mesh_id' columns to Data table");
        }

        // Purge legacy gateway status rows from Data table so they are not treated as mesh nodes
        try {
          const purgeRes = await database.run("DELETE FROM Data WHERE meshLevel < 0 OR sensorName = 'Gateway'");
          if (purgeRes && purgeRes.changes > 0) {
            console.log(`[db] Auto-cleaned: Purged ${purgeRes.changes} legacy gateway status rows from Data table`);
          }
        } catch (e) {
          console.warn("[db] Clean legacy gateway status rows warning:", e.message);
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

    const gwObj = pkt.gateway || {};
    const gw_id = String(pkt.gw_id || pkt.gwId || gwObj.gw_id || gwObj.id || "gw-default");
    const mesh_id = String(pkt.mesh_id || pkt.meshId || gwObj.mesh_id || "mesh-default");

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
          gw_id,
          mesh_id,
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
        gw_id,
        mesh_id,
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

    const placeholders = rows.map(() => "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").join(", ");
    const values = [];
    rows.forEach((r) => {
      values.push(
        r.mac,
        r.gw_id,
        r.mesh_id,
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
        mac, gw_id, mesh_id, deviceTime, serverTime, meshLevel, 
        schemaVersion, packetLoss, port, sensorType, sensorName, 
        field, value, nodeTimeValid, sourceIp
      ) VALUES ${placeholders}
    `;

    try {
      await activeDb.run(query, values);

      // Auto-register / update Gateway in Gateways table
      try {
        const gwObj = pkt.gateway || {};
        const gwId = String(pkt.gw_id || pkt.gwId || gwObj.gw_id || gwObj.id || "gw-default");
        const gwName = String(pkt.gw_name || gwObj.name || gwObj.gw_name || (gwId === "gw-default" ? "Trạm Gateway Chính" : gwId));
        const location = String(pkt.location || gwObj.location || (gwId === "gw-default" ? "Khu Vực Trung Tâm" : ""));
        const rootMac = String(pkt.root_mac || gwObj.root_mac || (Number(pkt.n) === 0 ? pkt.M || pkt.i : ""));
        const meshId = String(pkt.mesh_id || pkt.meshId || gwObj.mesh_id || "mesh-default");

        await upsertGateway({
          gw_id: gwId,
          name: gwName,
          location,
          root_mac: rootMac,
          mesh_id: meshId,
          ip: sourceIp,
          status: "online",
          last_seen: receivedAt.toISOString(),
        });
      } catch (e) {
        // Non-blocking
      }

      return rows.length;
    } catch (err) {
      console.error("[sqlite] insert failed:", err.message);
      return 0;
    }
  }

  async function persistGatewayStatus(pkt, sourceIp, receivedAt = new Date()) {
    if (!pkt || pkt.type !== "gateway_status") return 0;
    const serverTime = receivedAt.toISOString();
    const gw_id = String(pkt.gw_id || pkt.gwId || "gw-default");
    const mesh_id = String(pkt.mesh_id || pkt.meshId || "mesh-default");
    const gwName = String(pkt.name || pkt.gw_name || (gw_id === "gw-default" ? "Gateway 1 - Trạm Trung Tâm" : gw_id));
    const location = String(pkt.location || (gw_id === "gw-default" ? "Khu Vực Quản Lý Chính" : ""));
    const rootMac = String(pkt.mac || pkt.sta_mac || "");
    const staIp = String(pkt.sta_ip || sourceIp || "");

    try {
      await upsertGateway({
        gw_id,
        name: gwName,
        location,
        root_mac: rootMac,
        mesh_id,
        ip: staIp,
        status: "online",
        last_seen: serverTime,
      });
      return 1;
    } catch (err) {
      console.error("[sqlite] persistGatewayStatus failed:", err.message);
      return 0;
    }
  }

  async function upsertGateway({ gw_id, name, location, latitude = null, longitude = null, root_mac, mesh_id, ip, status = "online", last_seen = new Date().toISOString(), notes = "" }) {
    const activeDb = await ensureConnected();
    const id = String(gw_id || "gw-default").trim();
    const existing = await activeDb.get("SELECT * FROM Gateways WHERE gw_id = ?", [id]);
    const validIp = (ip && ip !== "127.0.0.1" && ip !== "0.0.0.0") ? ip : (existing?.ip || ip || "");
    const validMac = (root_mac && root_mac !== "gateway") ? root_mac : (existing?.root_mac || root_mac || "");
    if (existing) {
      await activeDb.run(
        `UPDATE Gateways SET 
          status = ?, 
          last_seen = ?, 
          ip = ?,
          root_mac = ?,
          mesh_id = COALESCE(NULLIF(?, ''), mesh_id)
        WHERE gw_id = ?`,
        [status, last_seen, validIp, validMac, mesh_id || "", id]
      );
    } else {
      await activeDb.run(
        `INSERT INTO Gateways (gw_id, name, location, latitude, longitude, root_mac, mesh_id, ip, status, last_seen, notes)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        [id, name || id, location || "", latitude, longitude, validMac, mesh_id || "", validIp, status, last_seen, notes || ""]
      );
    }
  }

  async function getGateways() {
    const activeDb = await ensureConnected();
    const gws = await activeDb.all("SELECT * FROM Gateways ORDER BY last_seen DESC");
    
    for (const gw of gws) {
      gw.mac = gw.root_mac || "";
      const countRes = await activeDb.get(
        "SELECT COUNT(DISTINCT mac) as cnt FROM Data WHERE gw_id = ? AND meshLevel >= 0",
        [gw.gw_id]
      );
      gw.node_count = countRes ? countRes.cnt : 0;
      
      const lastSeenMs = new Date(gw.last_seen).getTime();
      if (Date.now() - lastSeenMs > 60000) {
        gw.status = "offline";
      }
    }
    
    if (gws.length === 0) {
      const defaultGw = {
        gw_id: "gw-default",
        name: "Gateway 1 - Trạm Trung Tâm",
        location: "Khu Vực Quản Lý Chính",
        latitude: null,
        longitude: null,
        root_mac: "",
        mesh_id: "mesh-default",
        ip: "127.0.0.1",
        status: "online",
        last_seen: new Date().toISOString(),
        node_count: 0,
        notes: "Cổng Gateway mặc định của hệ thống"
      };
      await upsertGateway(defaultGw);
      return [defaultGw];
    }
    
    return gws;
  }

  async function deleteGateway(gwId) {
    const activeDb = await ensureConnected();
    const id = String(gwId || "").trim();
    if (!id) return 0;
    await activeDb.run("DELETE FROM Gateways WHERE gw_id = ?", [id]);
    await activeDb.run("DELETE FROM Data WHERE gw_id = ?", [id]);
    return 1;
  }

  async function deleteGateways(gwIds = []) {
    const activeDb = await ensureConnected();
    if (!Array.isArray(gwIds) || gwIds.length === 0) return 0;
    const placeholders = gwIds.map(() => "?").join(",");
    await activeDb.run(`DELETE FROM Gateways WHERE gw_id IN (${placeholders})`, gwIds);
    await activeDb.run(`DELETE FROM Data WHERE gw_id IN (${placeholders})`, gwIds);
    return gwIds.length;
  }

  async function deleteNodes(macs = []) {
    const activeDb = await ensureConnected();
    if (!Array.isArray(macs) || macs.length === 0) return 0;
    const placeholders = macs.map(() => "?").join(",");
    const res = await activeDb.run(
      `DELETE FROM Data WHERE mac IN (${placeholders}) OR sourceIp IN (${placeholders})`,
      [...macs, ...macs]
    );
    console.log(`[db] deleteNodes: deleted ${res?.changes || 0} rows for ${macs.length} nodes:`, macs);
    return res?.changes || macs.length;
  }

  async function updateGatewayMeta(gw_id, { name, location, latitude, longitude, notes }) {
    const activeDb = await ensureConnected();
    const id = String(gw_id || "").trim();
    const updates = [];
    const params = [];
    if (name !== undefined) { updates.push("name = ?"); params.push(name); }
    if (location !== undefined) { updates.push("location = ?"); params.push(location); }
    if (latitude !== undefined) { updates.push("latitude = ?"); params.push(latitude !== null && latitude !== "" ? Number(latitude) : null); }
    if (longitude !== undefined) { updates.push("longitude = ?"); params.push(longitude !== null && longitude !== "" ? Number(longitude) : null); }
    if (notes !== undefined) { updates.push("notes = ?"); params.push(notes); }
    if (!updates.length) return null;
    params.push(id);
    await activeDb.run(`UPDATE Gateways SET ${updates.join(", ")} WHERE gw_id = ?`, params);
    return await activeDb.get("SELECT * FROM Gateways WHERE gw_id = ?", [id]);
  }

  async function updateNodeMeta(mac, { name, location, latitude, longitude, notes }) {
    const activeDb = await ensureConnected();
    const id = String(mac || "").trim();
    if (!id) return null;
    const nowIso = new Date().toISOString();
    const existing = await activeDb.get("SELECT * FROM NodesMeta WHERE mac = ?", [id]);
    const latVal = latitude !== undefined && latitude !== null && latitude !== "" ? Number(latitude) : null;
    const lonVal = longitude !== undefined && longitude !== null && longitude !== "" ? Number(longitude) : null;

    if (existing) {
      const updates = ["updated_at = ?"];
      const params = [nowIso];
      if (name !== undefined) { updates.push("name = ?"); params.push(name); }
      if (location !== undefined) { updates.push("location = ?"); params.push(location); }
      if (latitude !== undefined) { updates.push("latitude = ?"); params.push(latVal); }
      if (longitude !== undefined) { updates.push("longitude = ?"); params.push(lonVal); }
      if (notes !== undefined) { updates.push("notes = ?"); params.push(notes); }
      params.push(id);
      await activeDb.run(`UPDATE NodesMeta SET ${updates.join(", ")} WHERE mac = ?`, params);
    } else {
      await activeDb.run(
        `INSERT INTO NodesMeta (mac, name, location, latitude, longitude, notes, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?)`,
        [id, name || id, location || "", latVal, lonVal, notes || "", nowIso]
      );
    }
    return await activeDb.get("SELECT * FROM NodesMeta WHERE mac = ?", [id]);
  }

  async function getNodesMeta() {
    const activeDb = await ensureConnected();
    return await activeDb.all("SELECT * FROM NodesMeta");
  }

  async function getNodeSnapshot(gw_id = null) {
    const activeDb = await ensureConnected();
    let query = `
      SELECT 
        d.mac as id,
        d.mac,
        d.gw_id,
        d.mesh_id,
        MAX(d.serverTime) as lastSeen,
        d.meshLevel,
        d.schemaVersion,
        d.packetLoss,
        nm.name as node_name,
        nm.location as node_location,
        nm.latitude,
        nm.longitude,
        nm.notes as node_notes
      FROM Data d
      LEFT JOIN NodesMeta nm ON d.mac = nm.mac
      WHERE (d.meshLevel >= 0 OR d.meshLevel IS NULL) AND (d.sensorName != 'Gateway' OR d.sensorName IS NULL)
    `;
    const params = [];
    if (gw_id && gw_id !== "all") {
      query += ` AND d.gw_id = ? `;
      params.push(gw_id);
    }
    query += `
      GROUP BY d.mac
      ORDER BY d.mac ASC
    `;
    const rows = await activeDb.all(query, params);

    return rows.map(r => ({
      ...r,
      gw_id: r.gw_id || "gw-default",
      mesh_id: r.mesh_id || "mesh-default",
      lastSeen: new Date(r.lastSeen)
    }));
  }

  async function getHistoryMacs(limit = 200) {
    const activeDb = await ensureConnected();
    const n = Math.max(1, Math.min(Number(limit) || 200, 2000));
    const rows = await activeDb.all(`
      SELECT mac, MAX(serverTime) as lastSeen, GROUP_CONCAT(DISTINCT sensorName) as sensors
      FROM Data
      WHERE (meshLevel >= 0 OR meshLevel IS NULL) AND (sensorName != 'Gateway' OR sensorName IS NULL)
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
    getGateways,
    upsertGateway,
    updateGatewayMeta,
    deleteGateway,
    deleteGateways,
    deleteNodes,
    updateNodeMeta,
    getNodesMeta,
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

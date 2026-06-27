-- ============================================================
-- 直播系统数据库索引优化脚本（幂等，可重复执行）
-- 用法：mysql -u root -p < init_database.sql
-- 说明：只添加缺失的索引，不修改已有表结构
-- ============================================================

USE live_system;

-- ============================================================
-- user 表索引
-- ============================================================
-- username 已有唯一索引（UNI），无需添加

-- ============================================================
-- room 表索引
-- ============================================================
-- room_no 已有唯一索引（UNI），无需添加
-- room_name 已有唯一索引（UNI），无需添加

-- 索引1：进房时同时查询房间号和直播状态（复合索引）
SET @dbname = DATABASE();
SET @tablename = 'room';
SET @indexname = 'idx_room_no_status';
SET @preparedStatement = (SELECT IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
   WHERE table_schema = @dbname
   AND table_name = @tablename
   AND index_name = @indexname) > 0,
  'SELECT 1',
  'ALTER TABLE `room` ADD INDEX `idx_room_no_status` (`room_no`, `live_status`)'
));
PREPARE alterIfNotExists FROM @preparedStatement;
EXECUTE alterIfNotExists;
DEALLOCATE PREPARE alterIfNotExists;

-- 索引2：主播下播查询（复合索引）
SET @indexname = 'idx_anchor_status';
SET @preparedStatement = (SELECT IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
   WHERE table_schema = @dbname
   AND table_name = @tablename
   AND index_name = @indexname) > 0,
  'SELECT 1',
  'ALTER TABLE `room` ADD INDEX `idx_anchor_status` (`anchor_uid`, `live_status`)'
));
PREPARE alterIfNotExists FROM @preparedStatement;
EXECUTE alterIfNotExists;
DEALLOCATE PREPARE alterIfNotExists;

-- 清理脏数据（room_no = 0 的无效记录）
DELETE FROM `room` WHERE `room_no` = 0;

-- 完成提示
SELECT '索引优化完成！' AS status;

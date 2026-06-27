-- ============================================================
-- 直播系统数据库初始化脚本（幂等，可重复执行）
-- 用法：mysql -u root -p < init_database.sql
-- ============================================================

-- 创建数据库
CREATE DATABASE IF NOT EXISTS live_system DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;

USE live_system;

-- ============================================================
-- user 表
-- ============================================================
CREATE TABLE IF NOT EXISTS `user` (
    `id` INT AUTO_INCREMENT PRIMARY KEY COMMENT '用户ID',
    `username` VARCHAR(40) NOT NULL COMMENT '昵称',
    `password` VARCHAR(40) NOT NULL COMMENT '密码',
    `tel` VARCHAR(40) NOT NULL COMMENT '手机号',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户表';

-- 索引：注册时查重、登录时查询（幂等）
SET @dbname = DATABASE();
SET @tablename = 'user';
SET @columnname = 'username';
SET @indexname = 'idx_username';
SET @preparedStatement = (SELECT IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
   WHERE table_schema = @dbname
   AND table_name = @tablename
   AND index_name = @indexname) > 0,
  'SELECT 1',
  CONCAT('ALTER TABLE `', @tablename, '` ADD UNIQUE INDEX `', @indexname, '` (`', @columnname, '`)')
));
PREPARE alterIfNotExists FROM @preparedStatement;
EXECUTE alterIfNotExists;
DEALLOCATE PREPARE alterIfNotExists;

-- ============================================================
-- room 表
-- ============================================================
CREATE TABLE IF NOT EXISTS `room` (
    `room_id` INT AUTO_INCREMENT PRIMARY KEY COMMENT '房间自增ID',
    `room_name` VARCHAR(64) NOT NULL COMMENT '房间名',
    `anchor_uid` BIGINT UNSIGNED NOT NULL COMMENT '主播用户ID',
    `live_status` TINYINT NOT NULL DEFAULT 1 COMMENT '直播状态: 0=下播 1=开播',
    `room_no` BIGINT UNSIGNED NOT NULL COMMENT '数字房间号(10位)',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `end_time` DATETIME DEFAULT NULL COMMENT '下播时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='房间表';

-- 索引1：观众进房查询（唯一索引）
SET @indexname = 'idx_room_no';
SET @columnname = 'room_no';
SET @preparedStatement = (SELECT IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
   WHERE table_schema = @dbname
   AND table_name = @tablename
   AND index_name = @indexname) > 0,
  'SELECT 1',
  CONCAT('ALTER TABLE `', @tablename, '` ADD UNIQUE INDEX `', @indexname, '` (`', @columnname, '`)')
));
PREPARE alterIfNotExists FROM @preparedStatement;
EXECUTE alterIfNotExists;
DEALLOCATE PREPARE alterIfNotExists;

-- 索引2：进房时同时查询房间号和直播状态（复合索引）
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

-- 索引3：主播下播查询
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
SELECT '数据库初始化完成！' AS status;

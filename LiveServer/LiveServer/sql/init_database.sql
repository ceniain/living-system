-- ============================================================
-- 直播系统数据库初始化脚本
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

-- 索引：注册时查重、登录时查询
ALTER TABLE `user` ADD UNIQUE INDEX `idx_username` (`username`);

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

-- 索引：观众进房查询（高频）
ALTER TABLE `room` ADD UNIQUE INDEX `idx_room_no` (`room_no`);

-- 复合索引：进房时同时查询房间号和直播状态
ALTER TABLE `room` ADD INDEX `idx_room_no_status` (`room_no`, `live_status`);

-- 索引：主播下播查询
ALTER TABLE `room` ADD INDEX `idx_anchor_status` (`anchor_uid`, `live_status`);

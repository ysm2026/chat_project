-- Chat 聊天室数据库（与线上一致）
-- 离线私聊约定见文件末尾「业务字段约定」

CREATE DATABASE IF NOT EXISTS chat_room;
USE chat_room;

CREATE TABLE IF NOT EXISTS user(
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT COMMENT '用户id',
    username VARCHAR(50) NOT NULL UNIQUE COMMENT '用户名',
    password_hash VARCHAR(50) NOT NULL COMMENT '用户密码',
    nickname VARCHAR(50) NOT NULL COMMENT '用户昵称',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    email VARCHAR(64) DEFAULT '' COMMENT '邮箱',
    phone VARCHAR(20) DEFAULT '' COMMENT '手机号',
    status TINYINT DEFAULT 1 COMMENT '账号状态：0=禁用,1=正常,2=注销',
    UNIQUE KEY uk_username(username),
    KEY idx_status(status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户信息表';

CREATE TABLE IF NOT EXISTS chat_room(
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT COMMENT '聊天室id',
    room_name VARCHAR(50) NOT NULL DEFAULT '' COMMENT '房间名',
    room_type TINYINT NOT NULL DEFAULT 2 COMMENT '房间类型：1=私聊,2=群聊',
    create_id INT UNSIGNED NOT NULL COMMENT '创建者id',
    announcement TEXT COMMENT '群公告',
    status TINYINT DEFAULT 1 COMMENT '状态：0=解散,1=正常',
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    FOREIGN KEY (create_id) REFERENCES user(id) ON DELETE CASCADE,
    KEY idx_creater(create_id),
    KEY idx_type_status(room_type, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='聊天室表';

CREATE TABLE IF NOT EXISTS room_number(
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '成员关系id',
    room_id INT UNSIGNED NOT NULL COMMENT '聊天室id',
    user_id INT UNSIGNED NOT NULL COMMENT '用户id',
    role TINYINT DEFAULT 3 COMMENT '成员角色：1=群主,2=管理员,3=普通成员',
    join_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
    last_read_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '最后一次读取消息的时间',
    status TINYINT DEFAULT 1 COMMENT '状态：0=已退出,1=正常,2=禁言',
    FOREIGN KEY (room_id) REFERENCES chat_room(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES user(id) ON DELETE CASCADE,
    UNIQUE (room_id, user_id),
    KEY idx_user(user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='聊天室成员表';

CREATE TABLE IF NOT EXISTS message(
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT COMMENT '消息id',
    send_id INT UNSIGNED NOT NULL COMMENT '发送者id',
    receiver_id INT UNSIGNED NULL COMMENT '接收者id',
    room_id INT NOT NULL COMMENT '房间id',
    content TEXT NOT NULL COMMENT '消息内容',
    message_type TINYINT DEFAULT 1 COMMENT '消息类型：1=文本,2=图片,3=文件,4=语音',
    send_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间',
    status TINYINT DEFAULT 1 COMMENT '消息状态：0=已撤回,1=正常,2=已删除',
    delivery_status TINYINT NOT NULL DEFAULT 1 COMMENT '投递状态：0=未投递,1=已投递',
    FOREIGN KEY (send_id) REFERENCES user(id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES user(id) ON DELETE CASCADE,
    KEY idx_room_time (room_id, send_time),
    KEY idx_sender(send_id),
    KEY idx_private_offline (receiver_id, room_id, delivery_status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='聊天消息表';

-- ============================================================
-- 若 message 表已存在但缺少 delivery_status，请执行：
-- ALTER TABLE message
--   ADD COLUMN delivery_status TINYINT NOT NULL DEFAULT 1
--   COMMENT '投递状态：0=未投递,1=已投递' AFTER status;
-- ALTER TABLE message
--   ADD KEY idx_private_offline (receiver_id, room_id, delivery_status);
-- ============================================================
--
-- 业务字段约定（与 ChatServer 代码一致）：
--   群聊消息：room_id = 聊天室id，receiver_id = NULL，message_type = 1，status = 1，delivery_status = 1
--   私聊消息：room_id = 0，receiver_id = 接收者id，message_type = 1(文本)，status = 1(正常)
--   离线私聊：delivery_status = 0；用户登录拉取后更新为 1
--

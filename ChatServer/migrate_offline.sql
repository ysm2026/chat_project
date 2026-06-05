-- 在已有 chat_room 库上启用「离线私聊」所需变更（执行一次即可）
USE chat_room;

ALTER TABLE message
  ADD COLUMN delivery_status TINYINT NOT NULL DEFAULT 1
  COMMENT '投递状态：0=未投递,1=已投递' AFTER status;

ALTER TABLE message
  ADD KEY idx_private_offline (receiver_id, room_id, delivery_status);

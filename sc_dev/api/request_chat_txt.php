<?php

include_once('common.php');
include_once('utils.php');

$mid = trim(avoid_sql($_POST['mid']));
$fuid = trim(avoid_sql($_POST['fuid']));
$fnick = trim(avoid_sql($_POST['fnick']));
$tuid = trim(avoid_sql($_POST['tuid']));
$content = trim(avoid_sql($_POST['content']));
$to_type = trim(avoid_sql($_POST['toType']));

$tag_type = "CHAT";

$res = array();


if (strlen($mid) > 0 &&
	strlen($fuid) > 0 &&
	strlen($fnick) > 0 &&
	strlen($tuid) > 0) {

    try {

        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		$tpid = 0;

		// ----- 查询是否已经是好友 -----
		// 查询关系表
		$group_name = "";
		$sql = "";
		if ($to_type == "user") {
			$sql = "SELECT id, pid, status FROM sc_relationship WHERE myid = {$tuid} AND fid = {$fuid} LIMIT 1";
		} else if ($to_type == "group") {
			$sql = "SELECT gmt.id, gt.group_name as group_name FROM sc_group_members as gmt, sc_groups as gt WHERE gmt.member_id = {$fuid} AND gmt.gid = {$tuid} AND gmt.gid = gt.id LIMIT 1";
		}
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$group_name = $row['group_name'];
		} else {
			$res = show_info('fail', '您没有权限向对方发送消息');
			echo json_encode($res);
			return 0;	
		}


		// ----- 操作 -----
		// 1. 写消息到文件队列
		$file = "";
		if ($to_type == "user") {
			$file = write_content_to_file_with_uid($tuid, $content);
		} else {
			$file = write_content_to_file_with_gid($tuid, $content);
		}
		if (strlen($file) <= 0) {
			$res = show_info('fail', '系统出错');
			echo json_encode($res);
			return;
		}
		
		// 1. 写消息到数据库
		if ($to_type == "user") {
			$fnickB64 = base64_encode($fnick);
			$qid = write_queue_to_db($db, $mid, $tag_type, $fuid, $fnickB64, $tuid, "txt", $file, strlen($content), "");
			if ($qid == -1) {
				$res = show_info('fail', '系统出错');
				echo json_encode($res);
				return;
			}

			// 2. 给对方发送通知 
			$ret = send_notice_to_uid("user", $fuid, $fnickB64, $tuid, $tpid, $tag_type, $qid);
			if ($ret != true) {
				log_info("send notice to tuid:{$tuid} tag_type:{$tag_type} fail");
			}   

			$res = show_info('succ', '处理成功');
			$res['qid'] = $qid;
			$res['queue_file'] = $file;
		} else {
			// 获取所有群组成员
			$fnickB64 = base64_encode($group_name);
			$members = array();
			$sql = "SELECT member_id FROM sc_group_members WHERE gid = {$tuid}";
			$stmt = $db->prepare($sql);
			$stmt->execute();
			while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
				$members[] = $row['member_id'];
			}

			$qids = write_queue_to_db_for_group($db, $members, $mid, $tag_type, $fuid, $fnickB64, $tuid, "txt", $file, strlen($content), "");
			if (count($qids) == 0) {
				$res = show_info('fail', '系统出错');
				echo json_encode($res);
				return;
			}

			// 2. 给对方发送通知
			foreach ($qids as $member_id => $qid) {
				if ($member_id == $fuid)
					continue;

				$ret = send_notice_to_uid("group", $tuid, $fnickB64, $member_id, $tpid, $tag_type, $qid);
			}

			$res = show_info('succ', '处理成功');
			$res['qid'] = end($qids);
			$res['queue_file'] = $file;
		}

        echo json_encode($res);
		return;

	} catch (PDOException $e) {
		log_info("find user fail: ".$e->getMessage());

        $res = show_info('fail', $e->getMessage());
        echo json_encode($res);
        return 1;
	}



}

$res['status'] = 'fail';
$res['des'] = 'parameters error';

echo json_encode($res);



?>

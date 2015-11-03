<?php

include_once('common.php');
include_once('utils.php');

$get_uer_info_for_uid = trim(avoid_sql($_POST['get_uer_info_for_uid']));
$user_where = trim(avoid_sql($_POST['where']));

$fuid = trim(avoid_sql($_POST['fuid']));
$tuid = trim(avoid_sql($_POST['tuid']));
$queue_type = trim(avoid_sql($_POST['queue_type']));
$expire = trim(avoid_sql($_POST['expire']));
$tag_type = trim(avoid_sql($_POST['tag_type']));
$since_id = trim(avoid_sql($_POST['since_id']));	// 取该id之后的数据
$before_id = trim(avoid_sql($_POST['before_id']));	// 取该id之前的数据
$order_by = trim(avoid_sql($_POST['order_by']));
$sort = trim(avoid_sql($_POST['sort']));


$res = array();

if ((strlen($fuid) > 0 && strlen($tag_type) > 0) || strlen($user_where) > 0) {

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
		
		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
		
		$queue = array();
		$sql = '';

		$select = "SELECT id, mid, tag_type, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, cdate, expire, fdel, tdel FROM sc_queue ";
		$where = '';
		if (strlen($fuid) > 0) {
			$where .= " fuid = {$fuid} ";	
		}
		if (strlen($tuid) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " tuid = {$tuid} ";
		}
		if (strlen($tag_type) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= "tag_type = '{$tag_type}' ";
		}
		if (strlen($queue_type) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " queue_type = '{$queue_type}' ";
		}
		if (strlen($expire) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " expire = {$expire} ";
		}
		if ($since_id > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " id > {$since_id} ";
		}
		if ($before_id > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " id < {$before_id} ";
		}
		if (strlen($order_by) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " ORDER BY {$order_by} ";
		}
		if (strlen($sort) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " {$sort} ";
		}
		if (strlen($user_where) > 0) {
			if (strlen($where) > 0)
				$where .= " AND ";
			$where .= " {$user_where} ";
		}

		if (strlen($where) > 0) {
			$sql = $select." WHERE ".$where;
		} else {
			$sql = $select;
		}


		$stmt = $db->prepare($sql);
		$stmt->execute();
		while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			
			$item = array();
			$item['id'] = $row['id'];
			$item['mid'] = $row['mid'];
			$item['tag_type'] = $row['tag_type'];
			$item['fuid'] = $row['fuid'];
			$item['fnick'] = $row['fnick'];
			$item['tuid'] = $row['tuid'];
			$item['queue_type'] = $row['queue_type'];
			$item['queue_file'] = $row['queue_file'];
			$item['queue_size'] = $row['queue_size'];
			$item['image_wh'] = $row['image_wh'];
			$item['cdate'] = $row['cdate'];
			$item['expire'] = $row['expire'];
			$item['fdel'] = $row['fdel'];
			$item['tdel'] = $row['tdel'];

			if ($row['queue_type'] == 'txt') {
				$item['content'] = file_get_contents($row['queue_file']);
			}

			$queue[] = $item;	

		}

		$res = show_info('succ', '查询成功');
		$res['data']['queue'] = $queue;

		$user_info = array();
		if (strlen($get_uer_info_for_uid) > 0) {
			$sql = "SELECT id, nickname, birthday, gender, status FROM sc_user WHERE id = {$get_uer_info_for_uid} LIMIT 1";
			$stmt = $db->prepare($sql);
			$stmt->execute();
			if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
				$user_info['uid'] = $row['id'];
				$user_info['nickname'] = $row['nickname'];
				$user_info['birthday'] = $row['birthday'];
				$user_info['gender'] = $row['gender'];
				$user_info['status'] = $row['status'];
				$user_info['icon'] = get_avatar_url($row['id']);
		
				$res['data']['user_info'] = $user_info;
			}
		}

		echo json_encode($res);

    	return 0;
		
		
	} catch (PDOException $e) {
		log_info("find user fail: ".$e->getMessage());
		
		$res = show_info('fail', $e->getMessage());
		echo json_encode($res);
		return 1;
	}


} else {
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}

echo json_encode($res);

?>


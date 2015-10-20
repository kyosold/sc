<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));

$res = array();

if (strlen($uid) > 0) {

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
		
		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
		
		$user = array();
		$sql = "SELECT id, nickname, birthday, gender, status FROM sc_user WHERE id = {$uid} LIMIT 1";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			
        	$user['id'] = $row['id'];
        	$user['nickname'] = $row['nickname'];
        	$user['birthday'] = $row['birthday'];
        	$user['gender'] = $row['gender'];
        	$user['status'] = $row['status'];
        	$user['icon'] = get_avatar_url($row['id']);
			
		} else {
			log_info("uid:{$uid} is not exits in the table sc_user");
			$res = show_info('fail', "用户{$uid}不存在");
			$res['sql'] = $sql;
			echo json_encode($res);
			return 1;

		}

		$res = show_info('succ', '查询成功');
		$res['user'] = $user;
    
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


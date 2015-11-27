<?php

include_once('common.php');
include_once('utils.php');


$group_name = trim(avoid_sql($_POST['group_name']));
$owner_id = trim(avoid_sql($_POST['owner_id']));
$members = trim(avoid_sql($_POST['members']));

$res = array();

if ((strlen($group_name) > 0) &&
	(strlen($owner_id) > 0) &&
	(strlen($members) > 0) ) {

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);

		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
		
		// 查询是否已经存在
		$sql = "SELECT id FROM sc_groups WHERE owner_id = {$owner_id} AND group_name = '{$group_name}' LIMIT 1";

		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch()) {
			log_info("make group fail, owner:{$owner_id} group name:{$group_name} had been exists.");

			$res = show_info('fail', '该群组已经存在.');
			$res['localdes'] = $sql;
			echo json_encode($res);

			return 0;
		}

	} catch( PDOException $e) {
		log_info("make group fail, ". $e->getMessage());

		$res = show_info('fail', $e->getMessage());
		echo json_encode($res);
		return 1;	
	}



	try {
		$db->beginTransaction();

		// 注册群组信息
		$sql = "INSERT INTO sc_groups (group_name, owner_id, status) ";
		$sql .= " VALUES ('{$group_name}', {$owner_id}, 0) ";

		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}
			
		$gid = $db->lastInsertId();
		//log_info("add to sc_user succ. uid:{$uid}");

		// 添加成员
		$sql = "INSERT INTO sc_group_members (gid, member_id) VALUES ({$gid}, {$owner_id}) ";
		$members_list = explode(",", $members);
		$i = 0;
		foreach ($members_list as $member) {
			$sql .= ", ({$gid}, {$member})";
		}	

		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}

		$pid = $db->lastInsertId();

		$db->commit();

		//log_info("add to sc_pwds succ. id:{$pid}");

	} catch (PDOException $e) {

		$db->rollback();
		log_info("make group fail, ".$e->getMessage());

		$res = show_info('fail', $e->getMessage());
		$res['localdes'] = $e->getMessage();
		echo json_encode($res);

		return 0;
	}


	//----- 上传头像 -----
	$avatar_path = DATA_PATH ."/groups/". $gid ."/avatar/";
	$avatar_img = $avatar_path ."avatar.jpg";
	$avatar_thumb = $avatar_path ."s_avatar.jpg";
	
	// 创建用户目录
	if (mk_dir($avatar_path) != TRUE) {

		log_info("make group fail, mkdir fail:{$avatar_path}");
		$res = show_info('fail', '系统出错(4004)，请稍候重试 :)');
		$res['localdes'] = $avatar_path;
		echo json_encode($res);

		return 1;
	}
	log_info("create user dir:{$avatar_path} succ. uid:{$uid}");

	// 检查头像是否有数据,有的话写文件
	$avatar_name = strtolower($_FILES['avatar']['name']);
	$avatar_size = $_FILES['avatar']['size'];

	if (strlen($avatar_name) > 0 && $avatar_size > 0) {
		if (move_uploaded_file($_FILES['avatar']['tmp_name'], $avatar_img)) {
			$img = new Imagick($avatar_img);
			$img->thumbnailImage(100, 100);	
			file_put_contents($avatar_thumb, $img);

			//log_info("upload avatar succ");

		} else {
			log_info("make group fail, move_uploaded_file ".$_FILES['avatar']['tmp_name']." to {$avatar_img} fail");

			unlink($avatar_path);
			//log_info("unlink succ:{$avatar_path}");

			$res = show_info('fail', '上传头像失败');
			echo json_encode($res);
			return 1;
		}
	} else {
		log_info("make group fail, avatar name or size less than 0");

		unlink($avatar_path);
		//log_info("unlink succ:{$avatar_path}");

		$res = show_info('fail', '上传头像失败');
		$res['localdes'] = 'avatar name or size less than 0';
		echo json_encode($res);
		return 1;
	}

	// 查询成员信息
	$g_member_list = array();
	$sql = "SELECT id, nickname, birthday, gender, status FROM sc_user WHERE id IN ({$members}) ";
	$stmt = $db->prepare($sql);
	$stmt->execute();
	while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
		$g_member = array();
		$g_member['uid'] = $row['id'];	
		$g_member['nickname'] = $row['nickname'];	
		$g_member['birthday'] = $row['birthday'];	
		$g_member['gender'] = $row['gender'];	
		$g_member['status'] = $row['status'];	
		$g_member['icon'] = get_avatar_url($row['id']);
		$g_member_list[] = $g_member;

	}
	

	//不管执行成功还是失败最后都要在关闭自动提交
	$db->setAttribute(PDO::ATTR_AUTOCOMMIT, 1);

	log_info("make group succ. uid:{$uid}");

	$group_data = array();
	$group_data['gid'] = $gid;
	$group_data['nickname'] = $group_name;
	$group_data['owner'] = $owner_id;
	$group_data['status'] = 0;
	$group_data['icon'] = get_group_avatar_url($gid);
	$group_data['members'] = $g_member_list;

	$res = show_info('succ', '注册成功!');
	$res['group'] = $group_data;
    echo json_encode($res);

	return 0;
	
} else {
	log_info("register fail, parameters error");

	$res['status'] = 'fail';
	$res['des'] = 'parameters error';

	echo json_encode($res);
}




?>

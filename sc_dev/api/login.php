<?php

include_once('common.php');
include_once('utils.php');

$account = trim(avoid_sql($_POST['account']));
$password = trim(avoid_sql($_POST['password']));
$ios_token = trim(avoid_sql($_POST['ios_token']));

$res = array();


if ( (strlen($account) > 0)
	&& (strlen($password) > 0) ) {

	$password = md5($password); 

	$mydata = array();
	$uid = 0;
	$pid = 0;

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);

		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		// 认证
		$sql = "SELECT id, uid FROM sc_pwds WHERE account = :account and passwd = :password LIMIT 1";
		
		$stmt = $db->prepare($sql);
		$stmt->bindParam(':account', $account, PDO::PARAM_STR);
		$stmt->bindParam(':password', $password, PDO::PARAM_STR);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$pid = $row['id'];
			$uid = $row['uid'];
		}

		if ($uid <= 0) {
			log_info("login fail, account:{$account} or passowrd:{$password} error");
			$res = show_info('fail', '登录失败, 帐号或密码错误');
			echo json_encode($res);
			return 0;
		}

		// 查询用户是否已经在线了
		$tpid = mc_get($uid);
		if ($tpid != false && strlen($tpid) > 0) {
			// 发送登录通知给正在在线用户
			// .....

			log_info("login fail, uid:{$uid} has online");
			$res = show_info('fail', '登录失败, 该帐号已经登录,请从另一设备上退出后再登录');
			echo json_encode($res);
			return 0;
		}
		
		// 查询用户信息
		$sql = "SELECT nickname, birthday, gender, status FROM sc_user WHERE id = :uid LIMIT 1";
	
		$stmt = $db->prepare($sql);
		$stmt->bindParam(':uid', $uid, PDO::PARAM_INT);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$mydata['id'] = $uid;
			$mydata['pid'] = $pid;
			$mydata['account'] = $account;
			$mydata['nickname'] = $row['nickname'];
			$mydata['birthday'] = $row['birthday'];
			$mydata['gender'] = $row['gender'];
			$mydata['status'] = $row['status'];
			$mydata['icon'] = get_avatar_url($uid);

		} else {
			log_info("login fail, get uid:{$uid} is null from table sc_user");
			$res = show_info('fail', '登录失败, 帐号或密码错误');
			echo json_encode($res);

			return 0;
		}

		// 登录成功，设置通知推送记录
		if ( strlen($ios_token) > 0 ) {
			$sql = "INSERT IGNORE INTO sc_logined (uid, ios_token) values (:uid, :ios_token)";
			$stmt = $db->prepare($sql);
			$stmt->bindParam(':uid', $uid, PDO::PARAM_INT);
			$stmt->bindParam(':ios_token', $ios_token, PDO::PARAM_STR);
			$stmt->execute();
		}

		// 查询好友信息
		$friends = array();

		$sql = "SELECT u.id as uid, u.nickname as nickname, u.birthday as birthday, u.gender as gender, r.pid as pid, r.status as status FROM sc_user AS u, sc_relationship AS r WHERE u.id = r.fid AND r.myid = :uid";
		
		$stmt = $db->prepare($sql);
		$stmt->bindParam(':uid', $uid, PDO::PARAM_INT);
		$stmt->execute();
		while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$friend_data = array();
			$friend_data['uid'] = $row['uid'];
			$friend_data['nickname'] = $row['nickname'];
			$friend_data['birthday'] = $row['birthday'];
			$friend_data['gender'] = $row['gender'];
			$friend_data['pid'] = $row['pid'];
			$friend_data['status'] = $row['status'];
			$friend_data['icon'] = get_avatar_url($row['uid']);

			$friends[] = $friend_data;
		}

		// 查询群组信息
		$groups = array();
		
		// get group id
		$group_ids = "";
		$sql = "SELECT gid FROM sc_group_members WHERE member_id = {$uid} GROUP BY gid ";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			if (strlen($group_ids) > 0) {
				$group_ids .= ",";
			}
			$group_ids .= $row['gid'];
		}

		if (strlen($group_ids) > 0) {
			// get group info
			$sql = "SELECT id, group_name, owner_id, status FROM sc_groups WHERE id in ({$group_ids}) ";
			$stmt = $db->prepare($sql);
			$stmt->execute();
			while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
				$group_info = array();
				$group_info['gid'] = $row['id'];		
				$group_info['nickname'] = $row['group_name'];		
				$group_info['owner'] = $row['owner_id'];		
				$group_info['icon'] = get_group_avatar_url($row['id']);		
				$group_info['status'] = $row['status'];		
				$group_info['members'] = array();
			
				$groups[] = $group_info;
			}

			// get member info joined groups
			$group_members = array();
			$sql = "SELECT gid, group_concat(member_id) as member_ids FROM sc_group_members WHERE gid IN ({$group_ids}) GROUP BY gid ";
			$stmt = $db->prepare($sql);
			$stmt->execute();
			while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
				$group_members[$row['gid']] = $row['member_ids'];	
			}

			// 查询成员信息
			foreach ($group_members as $gid => $member_ids) {
				$g_member_list = array();
				$sql = "SELECT id, nickname, birthday, gender, status FROM sc_user WHERE id in ({$member_ids})";
				$stmt = $db->prepare($sql);
				$stmt->execute();
				while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
					$g_member = array();
					$g_member['uid'] = $row['id'];  
					$g_member['nickname'] = $row['nickname'];   
					$g_member['birthday'] = $row['birthday'];   
					$g_member['gender'] = $row['gender'];   
					$g_member['status'] = $row['status'];   
					$g_member['icon'] = get_avatar_url($row['id']);;   
					$g_member_list[] = $g_member;
				}

				$i = 0;
				for ($i = 0; $i < count($groups); $i++) {
					if ($groups[$i]['gid'] == $gid) {
						$groups[$i]['members'] = $g_member_list;	
					}
				}
			}


		}	
		
		$res = show_info('succ', '登录成功');
		$res['myself'] = $mydata;
		$res['friends'] = $friends;
		$res['groups'] = $groups;
		echo json_encode($res);

		return 0;

	} catch( PDOException $e) {
		log_info("login fail: ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
		echo json_encode($res);
		return 1;
	}

	
} else {
	log_info("login fail, parameters error: account:{$account} password:{$password} ios_token:{$ios_token} ");

	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}



echo json_encode($res);


?>

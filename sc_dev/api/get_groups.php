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
		
		$res = show_info('succ', '查询成功');
		$res['groups'] = $groups;
		echo json_encode($res);

		return 0;

	} catch( PDOException $e) {
		log_info("get groups fail: ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
		echo json_encode($res);
		return 1;
	}

	
} else {
	log_info("get groups fail, parameters error: uid:{$uid} ");

	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}



echo json_encode($res);


?>

<?php

include_once('common.php');
include_once('utils.php');

$page = 0;

$uid = trim(avoid_sql($_POST['uid']));

$res = array();

if (strlen($uid) > 0) {

	$friends = array();

	try {
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		$sql = "SELECT u.id as uid, u.nickname as nickname, u.birthday as birthday, u.gender as gender, r.pid as pid, r.status as status FROM sc_user AS u, sc_relationship AS r WHERE u.id = r.fid AND r.myid = {$uid}";
		$stmt = $db->prepare($sql);
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

		$res = show_info('succ', '查询成功');
		$res['friends'] = $friends;

		echo json_encode($res);

		return 0;

	} catch (PDOException $e) {
        log_info("get friends fail: ".$e->getMessage());

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


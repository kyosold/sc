<?php

include_once('common.php');
include_once('utils.php');

$qid = 0;

$uid = trim(avoid_sql($_POST['uid']));
$qid = trim(avoid_sql($_POST['qid']));

$res = array();

if ((strlen($uid) > 0) && (strlen($qid) > 0)) {

	$queue = array();

	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);


		$sql = "";

		// 获取用户已读最大qid
		$db_qid = 0;
		$sql = "SELECT read_qid FROM sc_user WHERE id = {$uid} LIMIT 1";
		$stmt = $db->prepare($sql);
        $stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$db_qid = $row['read_qid'];	
		}	

		if ($qid == 0 && $db_qid == 0) {
			// 都为0时,不推送未读消息
			$res = show_info('succ', '处理成功');
			$res['queue'] = $queue;

			echo json_encode($res);

        	return 0;
		}

		if ($qid < $db_qid) {
			$qid = $db_qid;
		}


		$select = "SELECT id, mid, tag_type, gid, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, cdate, expire, fdel, tdel FROM sc_queue ";
		$where = " tuid = {$uid} AND id > {$qid} ";
		$sql = $select." WHERE ".$where;

		$stmt = $db->prepare($sql);
        $stmt->execute();
        while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {

            $item = array();
            $item['id'] = $row['id'];
            $item['mid'] = $row['mid'];
            $item['tag_type'] = $row['tag_type'];
			$item['gid'] = $row['gid'];
            $item['fuid'] = $row['fuid'];
            $item['fnick'] = $row['fnick'];
            $item['tuid'] = $row['tuid'];
            $item['queue_type'] = $row['queue_type'];
            $item['cdate'] = $row['cdate'];
            $item['expire'] = $row['expire'];
            $item['fdel'] = $row['fdel'];
            $item['tdel'] = $row['tdel'];

            $queue[] = $item;

        }

		$res = show_info('succ', '处理成功');
		$res['queue'] = $queue;


		echo json_encode($res);

        return 0;

	} catch (PDOException $e) {
		log_info("get unread message fail: ".$e->getMessage());

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


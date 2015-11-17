<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));
$nickname = trim(avoid_sql($_POST['nickname']));
$tuid = trim(avoid_sql($_POST['tuid']));

$res = array();

if (strlen($uid) > 0 && strlen($tuid) > 0) {

	 try {   
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
            
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
        
        $tag_type = "SYS_DFR";	// system delete friend relationship

		// 1. 删除好友关系记录
		$sql = "DELETE FROM sc_relationship WHERE (myid = {$uid} AND fid = {$tuid}) OR (myid = {$tuid} AND fid = {$uid}) LIMIT 2";
		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}
		
		$affected_rows = $stmt->rowCount();
		if ($affected_rows != 2) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}


		$need_notice = true;
		if ($need_notice == true) {
			$pid = "0";
			$nickname = "system";

			// 发送一个通知
			$ret = send_notice_to_uid($uid, $nickname, $tuid, $tpid, $tag_type, $pid);
			if ($ret == true) {
				$res = show_info('succ', '处理成功');
			} else {
				$res = show_info('succ', '处理成功');
			}
			echo json_encode($res);

			log_info("set friend action: send_notice_to_uid {$ret}");

			return 0;
			
		} else {
			log_info("set friend action fail: 请选择执行的动作");
			$res = show_info('fail', '请选择执行的动作');
			echo json_encode($res);
			return 0;
		}
	
	} catch (PDOException $e) {
		$db->rollback();
		
		log_info("set friend action fail, ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
        $res['localdes'] = $e->getMessage();
        echo json_encode($res);
		return 1;
	}		

    return 0;

} else {
	log_info("set friend action fail: parameters error");
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}




echo json_encode($res);

?>


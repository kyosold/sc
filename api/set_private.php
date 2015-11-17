<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));
$fuid = trim(avoid_sql($_POST['fuid']));
$password = trim(avoid_sql($_POST['password']));

$res = array();

if (strlen($uid) > 0 && strlen($fuid) > 0 && strlen($password) > 0) {

	 try {   
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
            
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		$password = md5($password);
		$pid = "";
		$account = "";
		$i = 0;

		// 检查是否该密码已经存在
		$sql = "SELECT id, account, passwd FROM sc_pwds WHERE uid = {$uid}";
		$stmt = $db->prepare($sql);
        $stmt->execute();
        while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			if (i >= MAX_HIDE_NUMBER) {	
				// 超限
				log_info("uid:{$uid} private number out of max number");
				$res = show_info('fail', '密码数量超限,请升级该帐号');
				echo json_encode($res);
				return 0;	
			}
			$i++;

			if ($row['passwd'] == $password) {
				// 密码已经存在, 使用该pid
				$pid = $row['id'];
			}

			$account = $row['account'];
		}

		if (strlen($account) == 0) {
			$res = show_info('fail', '统出错(4006)，请稍候重试 :)');
			echo json_encode($res);
			return 0;
		}


		if (strlen($pid) == 0) {
			// 密码不存在,也未超限,添加一个新的
			$sql = "INSERT INTO sc_pwds (account, passwd, uid) values ('{$account}', '{$password}', {$uid});";
			$stmt = $db->prepare($sql);
			if (!$stmt->execute()) {
				throw new PDOException('系统出错(2001)，请稍候重试 :)');
			}
			$pid = $db->lastInsertId();
		}

		if (strlen($pid) == 0) {
			$res = show_info('fail', '统出错(4008)，请稍候重试 :)');
			echo json_encode($res);
			return 0;
		}

		// 设置好友显示状态
		$sql = "UPDATE sc_relationship SET pid = {$pid} WHERE myid = {$uid} AND fid = {$fuid} LIMIT 1";
		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}
		$affected_rows = $stmt->rowCount();
		if ($affected_rows == 1) {
			$res = show_info('succ', '处理成功');
			$res['fpid'] = $pid;
			$res['fstatus'] = "1";
		} else {
			$res = show_info('fail', '处理失败');
		}

		echo json_encode($res);

		log_info("set friend action: send_notice_to_uid {$ret}");

		return 0;
			
	
	} catch (PDOException $e) {
		
		log_info("set private friend action fail, ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
        $res['localdes'] = $e->getMessage();
        echo json_encode($res);
		return 1;
	}		

    return 0;

} else {
	log_info("set private friend action fail: parameters error");
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}




echo json_encode($res);

?>

